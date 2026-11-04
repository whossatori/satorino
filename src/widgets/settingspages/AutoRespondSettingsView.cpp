// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/AutoRespondSettingsView.hpp"

#include "Application.hpp"
#include "controllers/autorespond/AutoRespondModel.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "providers/kick/KickChatServer.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QCheckBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimeEdit>
#include <QUuid>

#include <algorithm>
#include <tuple>

namespace chatterino::satorino {
namespace {

class ChannelDelegate : public QStyledItemDelegate
{
public:
    ChannelDelegate(bool timer, QObject *parent)
        : QStyledItemDelegate(parent)
        , timer_(timer)
    {
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        auto *combo = new QComboBox(parent);
        combo->addItem(this->timer_ ? "All connected channels" : "All channels",
                       "*");
        QStringList twitch, kick;
        getApp()->getTwitch()->forEachChannel([&](const ChannelPtr &channel) {
            if (channel->getType() == Channel::Type::Twitch)
                twitch << "twitch:" + channel->getName().toLower();
        });
        getApp()->getKickChatServer()->forEachChannel(
            [&](KickChannel &channel) {
                kick << "kick:" + channel.getName().toLower();
            });
        twitch.removeDuplicates();
        kick.removeDuplicates();
        twitch.sort();
        kick.sort();
        for (const auto &key : twitch + kick)
            combo->addItem(key, key);
        const auto saved = index.data(Qt::UserRole).toString();
        if (combo->findData(saved) < 0)
            combo->addItem(saved + " (closed)", saved);
        combo->addItem("YouTube (read-only)", "youtube:");
        if (auto *model = qobject_cast<QStandardItemModel *>(combo->model()))
            model->item(combo->count() - 1)->setEnabled(false);
        return combo;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *combo = static_cast<QComboBox *>(editor);
        combo->setCurrentIndex(combo->findData(index.data(Qt::UserRole)));
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QComboBox *>(editor)->currentData(),
                       Qt::UserRole);
    }

private:
    bool timer_;
};

class NumberDelegate : public QStyledItemDelegate
{
public:
    NumberDelegate(int min, int max, QObject *parent)
        : QStyledItemDelegate(parent)
        , min_(min)
        , max_(max)
    {
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *editor = new QSpinBox(parent);
        editor->setRange(this->min_, this->max_);
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QSpinBox *>(editor)->setValue(
            index.data(Qt::EditRole).toInt());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QSpinBox *>(editor)->value(),
                       Qt::EditRole);
    }

private:
    int min_;
    int max_;
};

class TimeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *editor = new QTimeEdit(parent);
        editor->setDisplayFormat("HH:mm");
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        const auto time = QTime::fromString(index.data().toString(), "HH:mm");
        static_cast<QTimeEdit *>(editor)->setTime(
            time.isValid() ? time : QTime(12, 0));
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(
            index, static_cast<QTimeEdit *>(editor)->time().toString("HH:mm"),
            Qt::EditRole);
    }
};

class DaysDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *editor = new QWidget(parent);
        auto *layout = new QHBoxLayout(editor);
        layout->setContentsMargins(0, 0, 0, 0);
        const QStringList names{"M", "Tu", "W", "Th", "F", "Sa", "Su"};
        for (int i = 0; i < 7; ++i)
        {
            auto *box = new QCheckBox(names[i], editor);
            box->setProperty("dayBit", 1 << i);
            layout->addWidget(box);
        }
        editor->setAutoFillBackground(true);
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        const auto days = index.data(Qt::UserRole).toInt();
        for (auto *box : editor->findChildren<QCheckBox *>())
            box->setChecked(days & box->property("dayBit").toInt());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        int days = 0;
        for (auto *box : editor->findChildren<QCheckBox *>())
            if (box->isChecked())
                days |= box->property("dayBit").toInt();
        model->setData(index, days, Qt::UserRole);
    }
};

void prepareTable(EditableModelView *view)
{
    view->setMinimumWidth(0);
    view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *table = view->getTableView();
    table->setMinimumWidth(0);
    table->setMinimumHeight(220);
    table->setMaximumHeight(320);
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setDefaultSectionSize(125);
}

}  // namespace

void addAutoRespondSettings(GeneralPageView &view)
{
    auto *available = tryAutoRespondSettings();
    if (!available)
    {
        view.addTitle("Auto respond");
        view.addDescription("Available after application startup.");
        return;
    }
    auto &settings = *available;
    view.addTitle("Auto respond");
    SettingWidget::checkbox("Enable auto respond", settings.enabled)
        ->addTo(view);
    SettingWidget::intInput("Maximum auto-sent messages per minute",
                            settings.perMinute, {.min = 1, .max = 60})
        ->addTo(view);
    SettingWidget::checkbox("Show a note when auto respond sends",
                            settings.showNote)
        ->addTo(view);
    view.addDescription("Replies to live messages in Twitch and Kick channels. "
                        "Your own messages and chat history are ignored. "
                        "/autorespond off stops all rules immediately. "
                        "YouTube is read-only. New rules start disabled.");
    view.addSubtitle("Triggers");
    auto *triggerModel = new AutoRespondTriggerModel(settings.triggerRows);
    auto *triggers = new EditableModelView(triggerModel);
    triggers->setTitles({"Enabled", "Label", "Channel", "Pattern", "Regex",
                         "Case-sensitive", "Response", "Play sound",
                         "Custom sound", "Cooldown (s)", "Delay (ms)"});
    prepareTable(triggers);
    auto *table = triggers->getTableView();
    table->setItemDelegateForColumn(AutoRespondTriggerModel::Channel,
                                    new ChannelDelegate(false, table));
    table->setItemDelegateForColumn(AutoRespondTriggerModel::Cooldown,
                                    new NumberDelegate(1, 86400, table));
    table->setItemDelegateForColumn(AutoRespondTriggerModel::Delay,
                                    new NumberDelegate(0, 3600000, table));
    table->setColumnWidth(AutoRespondTriggerModel::Pattern, 220);
    table->setColumnWidth(AutoRespondTriggerModel::Response, 220);
    triggers->addRegexHelpLink();
    std::ignore = triggers->addButtonPressed.connect([&settings] {
        AutoRespondTrigger rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.triggerRows.append(rule);
    });
    QObject::connect(
        table, &QTableView::doubleClicked, triggers,
        [triggers, triggerModel](const QModelIndex &clicked) {
            if (clicked.column() != AutoRespondTriggerModel::CustomSound)
                return;
            const QPersistentModelIndex index(clicked);
            const auto file = QFileDialog::getOpenFileName(
                triggers, "Choose auto respond sound", {},
                "Audio files (*.wav *.mp3 *.ogg *.flac);;All files (*)");
            if (!file.isEmpty() && index.isValid())
                triggerModel->setData(index, QUrl::fromLocalFile(file),
                                      Qt::UserRole);
        });
    auto *defaultSound = new QPushButton("Use default sound");
    triggers->addCustomButton(defaultSound);
    QObject::connect(
        defaultSound, &QPushButton::clicked, triggers, [table, triggerModel] {
            const auto selected = table->selectionModel()->selectedRows();
            if (!selected.isEmpty())
                triggerModel->setData(
                    triggerModel->index(selected.front().row(),
                                        AutoRespondTriggerModel::CustomSound),
                    QUrl(), Qt::UserRole);
        });
    view.addWidget(triggers, {"auto respond", "triggers", "pattern", "response",
                              "cooldown", "sound", "channel"});
    view.addSubtitle("Timers");
    auto *timerModel = new AutoRespondTimerModel(settings.timerRows);
    auto *timers = new EditableModelView(timerModel);
    timers->setTitles(
        {"Enabled", "Label", "Channel", "Message", "Time (local)", "Days"});
    prepareTable(timers);
    auto *timerTable = timers->getTableView();
    timerTable->setItemDelegateForColumn(AutoRespondTimerModel::Channel,
                                         new ChannelDelegate(true, timerTable));
    timerTable->setItemDelegateForColumn(AutoRespondTimerModel::Time,
                                         new TimeDelegate(timerTable));
    timerTable->setItemDelegateForColumn(AutoRespondTimerModel::Days,
                                         new DaysDelegate(timerTable));
    timerTable->setColumnWidth(AutoRespondTimerModel::Message, 260);
    timerTable->setColumnWidth(AutoRespondTimerModel::Days, 350);
    std::ignore = timers->addButtonPressed.connect([&settings] {
        AutoRespondTimer rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.timerRows.append(rule);
    });
    view.addDescription("Timers use local HH:mm time and selected weekdays. "
                        "Each rule fires at most once "
                        "per day. Closed or disconnected channels and times "
                        "missed while the app was stopped or asleep "
                        "are skipped; they are never sent on reconnect.");
    view.addWidget(timers, {"auto respond", "timers", "time", "days", "message",
                            "channel"});
}
}  // namespace chatterino::satorino
