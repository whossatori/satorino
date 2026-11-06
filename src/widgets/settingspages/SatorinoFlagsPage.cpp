// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"
#include "controllers/flags/SatorinoFlags.hpp"
#include "singletons/Settings.hpp"
#include "util/SatorinoFlags.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include <QAbstractItemView>
#include <QColor>
#include <QColorDialog>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace chatterino {
void SatorinoPage::addLocalFlags(GeneralPageView &layout)
{
    layout.addTitle("Flagged users");
    layout.addDescription("Local to this device and channel. Flags never "
                          "report or restrict users on Twitch.");
    SettingWidget::checkbox("Enable local user flags",
                            getSettings()->satorinoFlagsEnabled)
        ->addKeywords({"flag", "user", "highlight"})
        ->addTo(layout);
    SettingWidget::checkbox("Show flag mark before the name",
                            getSettings()->satorinoFlagsMark)
        ->conditionallyEnabledBy(getSettings()->satorinoFlagsEnabled)
        ->addTo(layout);
    auto *group = new QWidget;
    auto *box = new QVBoxLayout(group);
    box->setContentsMargins(0, 0, 0, 0);
    auto *actions = new QHBoxLayout;
    auto *colour = new QPushButton("Highlight colour");
    colour->setToolTip("Subtle fallback colour; mentions, rewards and existing "
                       "highlights keep priority. Alpha is preserved.");
    actions->addWidget(colour);
    auto *clear = new QPushButton("Clear all");
    actions->addWidget(clear);
    actions->addStretch(1);
    box->addLayout(actions);
    auto *table = new QTableWidget(0, 4);
    table->setHorizontalHeaderLabels({"Channel", "User", "Date", ""});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    table->setMaximumHeight(190);
    box->addWidget(table);
    auto refresh = [table, clear] {
        const auto entries = satorino::localFlags().entries();
        table->setRowCount(0);
        for (const auto &entry : entries)
        {
            const int row = table->rowCount();
            table->insertRow(row);
            auto *channel = new QTableWidgetItem(entry.channelLabel.isEmpty()
                                                     ? entry.roomId
                                                     : entry.channelLabel);
            channel->setToolTip("Twitch room ID: " + entry.roomId);
            table->setItem(row, 0, channel);
            auto *user = new QTableWidgetItem(
                entry.userLabel.isEmpty() ? entry.userId : entry.userLabel);
            user->setToolTip("Twitch user ID: " + entry.userId);
            table->setItem(row, 1, user);
            table->setItem(
                row, 2,
                new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(entry.atMs)
                                         .toString("yyyy-MM-dd")));
            auto *remove = new QPushButton("Remove");
            QObject::connect(remove, &QPushButton::clicked, table,
                             [room = entry.roomId, user = entry.userId] {
                                 satorino::removeLocalFlag(room, user);
                             });
            table->setCellWidget(row, 3, remove);
        }
        clear->setEnabled(!entries.isEmpty());
    };
    getSettings()->satorinoFlagsState.connect(
        [table, refresh](const QString &) {
            // Do not delete the Remove sender during synchronous setting notification.
            QTimer::singleShot(0, table, refresh);
        },
        this->managedConnections_, false);
    getSettings()->satorinoFlagsEnabled.connect(
        [colour](bool enabled) {
            colour->setEnabled(enabled);
        },
        this->managedConnections_);
    getSettings()->satorinoFlagsColour.connect(
        [colour](const QString &value) {
            const QColor parsed(value);
            const QColor chosen = parsed.isValid()
                                      ? parsed
                                      : QColor(satorino::FLAGS_COLOUR_DEFAULT);
            colour->setStyleSheet(
                QString("background-color: rgba(%1,%2,%3,%4);")
                    .arg(chosen.red())
                    .arg(chosen.green())
                    .arg(chosen.blue())
                    .arg(chosen.alpha()));
        },
        this->managedConnections_);
    QObject::connect(colour, &QPushButton::clicked, this, [this] {
        const QPointer<SatorinoPage> guard(this);
        const QColor stored(getSettings()->satorinoFlagsColour.getValue());
        const auto chosen = QColorDialog::getColor(
            stored.isValid() ? stored : QColor(satorino::FLAGS_COLOUR_DEFAULT),
            this, "Local flag highlight colour",
            QColorDialog::ShowAlphaChannel);
        if (guard && chosen.isValid())
            getSettings()->satorinoFlagsColour.setValue(
                chosen.name(QColor::HexArgb));
    });
    QObject::connect(clear, &QPushButton::clicked, this, [this] {
        const QPointer<SatorinoPage> guard(this);
        if (QMessageBox::question(
                this, "Clear local flags",
                "Remove all locally flagged users in every channel?",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) == QMessageBox::Yes &&
            guard)
            getSettings()->satorinoFlagsState.setValue(
                satorino::FlagStore{}.toJson());
    });
    refresh();
    layout.addWidget(group, {"flagged users", "channel", "user", "date",
                             "remove", "clear", "highlight colour"});
}
}  // namespace chatterino
