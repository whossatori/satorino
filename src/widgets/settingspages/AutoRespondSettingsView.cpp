// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/AutoRespondSettingsView.hpp"
#include "Application.hpp"
#include "controllers/autorespond/AutoRespondController.hpp"
#include "controllers/autorespond/AutoRespondModel.hpp"
#include "providers/kick/KickChatServer.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/layout/FlowLayout.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QApplication>
#include <QScopedValueRollback>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QTableView>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <tuple>

namespace chatterino::satorino {
namespace {
class EnabledDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        auto background = option;
        this->initStyleOption(&background, index);
        background.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        QStyledItemDelegate::paint(painter, background, QModelIndex());
        QStyleOptionButton box;
        box.state = QStyle::State_Enabled |
                    (index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                         ? QStyle::State_On
                         : QStyle::State_Off);
        box.palette = option.palette;
        auto *style =
            option.widget ? option.widget->style() : QApplication::style();
        const int size = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr,
                                            option.widget);
        box.rect = QRect(option.rect.center() - QPoint(size / 2, size / 2),
                         QSize(size, size));
        style->drawControl(QStyle::CE_CheckBox, &box, painter, option.widget);
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        const bool click =
            event->type() == QEvent::MouseButtonRelease &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton &&
            option.rect.contains(
                static_cast<QMouseEvent *>(event)->position().toPoint());
        const bool key =
            event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Space;
        if (!click && !key)
            return false;
        return model->setData(
            index,
            index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                ? Qt::Unchecked
                : Qt::Checked,
            Qt::CheckStateRole);
    }
};

class RuleDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        if (index.column() == AutoRespondModel::Type)
        {
            auto *combo = new QComboBox(parent);
            combo->addItem("Trigger", 0);
            combo->addItem("Timer", 1);
            return combo;
        }
        if (index.column() == AutoRespondModel::Channel)
        {
            auto *combo = new QComboBox(parent);
            combo->setSizeAdjustPolicy(
                QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(8);
            combo->addItem("All connected channels", "*");
            QStringList twitch, kick;
            // Lookups do not construct a runtime, and teardown only shows the saved value.
            if (tryAutoRespond())
                if (auto *app = tryGetApp())
                {
                    if (auto *server = app->getTwitch())
                        server->forEachChannel([&](const ChannelPtr &channel) {
                            if (channel &&
                                channel->getType() == Channel::Type::Twitch)
                                twitch
                                    << "twitch:" + channel->getName().toLower();
                        });
                    if (auto *server = app->getKickChatServer())
                        server->forEachChannel([&](KickChannel &channel) {
                            kick << "kick:" + channel.getName().toLower();
                        });
                }
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
            if (auto *items =
                    qobject_cast<QStandardItemModel *>(combo->model()))
                items->item(combo->count() - 1)->setEnabled(false);
            return combo;
        }
        const auto *model =
            dynamic_cast<const AutoRespondModel *>(index.model());
        const auto rule = model ? model->ruleAt(index.row()) : std::nullopt;
        if (index.column() == AutoRespondModel::When && rule &&
            rule->type == AutoRespondType::Timer)
        {
            auto *edit = new QTimeEdit(parent);
            edit->setDisplayFormat("HH:mm");
            return edit;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            combo->setCurrentIndex(combo->findData(index.data(
                index.column() == AutoRespondModel::Channel ? Qt::UserRole
                                                            : Qt::EditRole)));
        else if (auto *time = qobject_cast<QTimeEdit *>(editor))
            time->setTime(QTime::fromString(index.data().toString(), "HH:mm"));
        else
            QStyledItemDelegate::setEditorData(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            model->setData(index, combo->currentData(),
                           index.column() == AutoRespondModel::Channel
                               ? Qt::UserRole
                               : Qt::EditRole);
        else if (auto *time = qobject_cast<QTimeEdit *>(editor))
            model->setData(index, time->time().toString("HH:mm"), Qt::EditRole);
        else
            QStyledItemDelegate::setModelData(editor, model, index);
    }
};

QWidget *field(const QString &label, QWidget *editor, QWidget *parent)
{
    auto *card = new QWidget(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *caption = new QLabel(label, card);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    layout->addWidget(editor);
    card->setMaximumWidth(190);
    card->setMinimumWidth(0);
    return card;
}

class AutoRespondEditor : public QWidget
{
public:
    explicit AutoRespondEditor(AutoRespondSettings &settings)
        : settings_(settings)
        , model_(new AutoRespondModel(settings, this))
    {
        this->setMinimumWidth(0);
        this->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *view = new EditableModelView(this->model_);
        view->addRegexHelpLink();
        layout->addWidget(view);
        this->table_ = view->getTableView();
        auto *table = this->table_;
        table->setItemDelegate(new RuleDelegate(table));
        table->setItemDelegateForColumn(AutoRespondModel::Enabled,
                                        new EnabledDelegate(table));
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setDragDropMode(QAbstractItemView::NoDragDrop);
        table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        table->setMinimumWidth(0);
        table->setMinimumHeight(180);
        table->setMaximumHeight(250);
        table->setTextElideMode(Qt::ElideRight);
        table->horizontalHeader()->setMinimumSectionSize(20);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->viewport()->installEventFilter(this);
        std::ignore = view->addButtonPressed.connect([this] {
            this->model_->addRule();
        });
        // Details use a wrapping layout, not additional wide table columns.
        this->description_ =
            new QLabel("Select an auto response to edit its options.", this);
        this->description_->setWordWrap(true);
        layout->addWidget(this->description_);
        this->triggers_ = new QWidget(this);
        auto *triggerFlow = new FlowLayout(this->triggers_, {0, 12, 8});
        this->regex_ = new QCheckBox("Regex", this);
        triggerFlow->addWidget(this->regex_);
        this->case_ = new QCheckBox("Case-sensitive", this);
        triggerFlow->addWidget(this->case_);
        this->sound_ = new QCheckBox("Play sound", this);
        triggerFlow->addWidget(this->sound_);
        this->cooldown_ = new QSpinBox(this);
        this->cooldown_->setRange(1, 86400);
        triggerFlow->addWidget(field("Cooldown (s)", this->cooldown_, this));
        this->delay_ = new QSpinBox(this);
        this->delay_->setRange(0, 3600000);
        triggerFlow->addWidget(field("Delay (ms)", this->delay_, this));
        this->soundFile_ = new QLineEdit(this);
        this->soundFile_->setReadOnly(true);
        this->soundFile_->setMinimumWidth(0);
        triggerFlow->addWidget(field("Custom sound", this->soundFile_, this));
        auto *choose = new QPushButton("Choose sound", this);
        triggerFlow->addWidget(choose);
        auto *defaultSound = new QPushButton("Use default sound", this);
        triggerFlow->addWidget(defaultSound);
        layout->addWidget(this->triggers_);
        this->timers_ = new QWidget(this);
        auto *timerFlow = new FlowLayout(this->timers_, {0, 12, 8});
        this->time_ = new QTimeEdit(this);
        this->time_->setDisplayFormat("HH:mm");
        timerFlow->addWidget(field("Time (local)", this->time_, this));
        const QStringList days{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        for (int i = 0; i < 7; ++i)
        {
            this->days_[i] = new QCheckBox(days[i], this);
            timerFlow->addWidget(this->days_[i]);
            QObject::connect(
                this->days_[i], &QCheckBox::toggled, this, [this, i](bool on) {
                    this->mutate([i, on](auto &r) {
                        r.days = on ? r.days | (1 << i) : r.days & ~(1 << i);
                    });
                });
        }
        layout->addWidget(this->timers_);
        QObject::connect(this->regex_, &QCheckBox::toggled, this,
                         [this](bool on) {
                             this->mutate([on](auto &r) {
                                 r.regex = on;
                             });
                         });
        QObject::connect(this->case_, &QCheckBox::toggled, this,
                         [this](bool on) {
                             this->mutate([on](auto &r) {
                                 r.caseSensitive = on;
                             });
                         });
        QObject::connect(this->sound_, &QCheckBox::toggled, this,
                         [this](bool on) {
                             this->mutate([on](auto &r) {
                                 r.sound = on;
                             });
                         });
        QObject::connect(this->cooldown_, &QSpinBox::editingFinished, this,
                         [this] {
                             const int n = this->cooldown_->value();
                             this->mutate([n](auto &r) {
                                 r.cooldownSeconds = n;
                             });
                         });
        QObject::connect(this->delay_, &QSpinBox::editingFinished, this,
                         [this] {
                             const int n = this->delay_->value();
                             this->mutate([n](auto &r) {
                                 r.delayMs = n;
                             });
                         });
        QObject::connect(
            this->time_, &QTimeEdit::editingFinished, this, [this] {
                const auto value = this->time_->time().toString("HH:mm");
                this->mutate([value](auto &r) {
                    r.time = value;
                });
            });
        QObject::connect(defaultSound, &QPushButton::clicked, this, [this] {
            this->mutate([](auto &r) {
                r.customSound.clear();
            });
        });
        QObject::connect(choose, &QPushButton::clicked, this, [this] {
            const auto id = this->selectedId_;
            const QPointer<AutoRespondEditor> guard(this);
            const auto file = QFileDialog::getOpenFileName(
                this, "Choose auto respond sound", {},
                "Audio files (*.wav *.mp3 *.ogg *.flac);;All files (*)");
            if (!guard || file.isEmpty())
                return;
            auto rule = guard->model_->ruleAt(guard->model_->rowForId(id));
            if (rule)
            {
                rule->customSound = QUrl::fromLocalFile(file).toString();
                guard->model_->updateRule(*rule);
            }
        });
        QObject::connect(
            table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &index, const QModelIndex &) {
                if (this->resetting_)
                    return;
                const auto rule = this->model_->ruleAt(index.row());
                this->selectedId_ = rule ? rule->id : QString();
                this->refresh();
            });
        QObject::connect(this->model_, &QAbstractItemModel::dataChanged, this,
                         [this] {
                             this->refresh();
                         });
        QObject::connect(this->model_, &QAbstractItemModel::modelAboutToBeReset,
                         this, [this] {
                             this->resetting_ = true;
                         });
        QObject::connect(
            this->model_, &QAbstractItemModel::modelReset, this, [this] {
                this->resetting_ = false;
                const int row = this->model_->rowForId(this->selectedId_);
                if (row >= 0)
                    this->table_->selectRow(row);
                this->refresh();
            });
        this->refresh();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == this->table_->viewport() &&
            event->type() == QEvent::Resize)
        {
            const int width = this->table_->viewport()->width();
            const int fixed = std::min(56, std::max(20, width / 8));
            const int rest = std::max(80, width - 2 * fixed);
            const int label = rest * 14 / 100, channel = rest * 23 / 100,
                      when = rest * 23 / 100;
            const std::array<int, 6> widths{
                fixed,   label, fixed,
                channel, when,  rest - label - channel - when};
            for (int i = 0; i < 6; ++i)
                this->table_->setColumnWidth(i, widths[i]);
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void mutate(const std::function<void(AutoRespondRule &)> &change)
    {
        if (this->binding_)
            return;
        auto rule =
            this->model_->ruleAt(this->model_->rowForId(this->selectedId_));
        if (!rule)
            return;
        change(*rule);
        this->model_->updateRule(*rule);
    }
    void refresh()
    {
        const QScopedValueRollback guard(this->binding_, true);
        const auto rule =
            this->model_->ruleAt(this->model_->rowForId(this->selectedId_));
        this->triggers_->setVisible(rule &&
                                    rule->type == AutoRespondType::Trigger);
        this->timers_->setVisible(rule && rule->type == AutoRespondType::Timer);
        if (!rule)
        {
            this->description_->setText(
                "Select an auto response to edit its options.");
            return;
        }
        const auto expression = autoRespondExpression(*rule);
        this->description_->setText(
            rule->type == AutoRespondType::Trigger && rule->regex &&
                    !expression.isValid()
                ? "Invalid regex: " + expression.errorString()
                : "Options for the selected auto response. Changes apply when "
                  "edited.");
        this->regex_->setChecked(rule->regex);
        this->case_->setChecked(rule->caseSensitive);
        this->sound_->setChecked(rule->sound);
        this->cooldown_->setValue(rule->cooldownSeconds);
        this->delay_->setValue(rule->delayMs);
        this->soundFile_->setText(rule->customSound.isEmpty()
                                      ? "Default sound"
                                      : QUrl(rule->customSound).fileName());
        this->soundFile_->setToolTip(rule->customSound);
        const auto time = QTime::fromString(rule->time, "HH:mm");
        this->time_->setTime(time.isValid() ? time : QTime(12, 0));
        for (int i = 0; i < 7; ++i)
            this->days_[i]->setChecked(rule->days & (1 << i));
    }
    AutoRespondSettings &settings_;
    AutoRespondModel *model_;
    QTableView *table_ = nullptr;
    QString selectedId_;
    bool binding_ = false, resetting_ = false;
    QLabel *description_ = nullptr;
    QWidget *triggers_ = nullptr, *timers_ = nullptr;
    QCheckBox *regex_ = nullptr, *case_ = nullptr, *sound_ = nullptr;
    QSpinBox *cooldown_ = nullptr, *delay_ = nullptr;
    QLineEdit *soundFile_ = nullptr;
    QTimeEdit *time_ = nullptr;
    std::array<QCheckBox *, 7> days_{};
};
}  // namespace

void addAutoRespondSettings(GeneralPageView &view)
{
    view.addTitle("Auto respond");
    auto *settings = tryAutoRespondSettings();
    if (!settings)
    {
        view.addDescription("Available after application startup.");
        return;
    }
    SettingWidget::checkbox("Enable auto respond", settings->enabled)
        ->addTo(view);
    SettingWidget::intInput("Maximum auto-sent messages per minute",
                            settings->perMinute, {.min = 1, .max = 60})
        ->addTo(view);
    SettingWidget::checkbox("Show a note when auto respond sends",
                            settings->showNote)
        ->addTo(view);
    view.addDescription("Replies to live messages in Twitch and Kick channels. "
                        "Your own messages and history are ignored. "
                        "/autorespond off stops all rules immediately. YouTube "
                        "is read-only. New rules start disabled.");
    view.addSubtitle("Auto responses");
    view.addWidget(new AutoRespondEditor(*settings),
                   {"auto respond", "auto responses", "trigger", "timer",
                    "pattern", "response", "channel", "regex", "case", "sound",
                    "cooldown", "delay", "time", "days"});
}
}  // namespace chatterino::satorino
