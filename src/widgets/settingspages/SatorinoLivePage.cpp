// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"

#include "Application.hpp"
#include "controllers/notifications/NotificationController.hpp"
#include "controllers/notifications/NotificationModel.hpp"
#include "singletons/Settings.hpp"
#include "util/SatorinoLive.hpp"
#include "util/Twitch.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QCheckBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QTableView>

namespace chatterino {

void SatorinoPage::addLiveStatus(GeneralPageView &layout)
{
    layout.addTitle("Live status");
    layout.addDescription(
        "Notifications work only while satorino is running. Watched streamers "
        "use the same list as Notifications -> Selected Channels, including "
        "channels without an open tab.");
    layout.addDescription(
        "Games to highlight (one exact game name or ID per line)");
    auto *games = new QPlainTextEdit;
    games->setMaximumHeight(100);
    games->setPlaceholderText("One game name per line");
    getSettings()->satorinoLiveGames.connect(
        [games](const QString &value) {
            if (games->toPlainText() != value)
            {
                const QSignalBlocker blocker(games);
                games->setPlainText(value);
            }
        },
        this->managedConnections_);
    QObject::connect(games, &QPlainTextEdit::textChanged, this, [games] {
        getSettings()->satorinoLiveGames.setValue(games->toPlainText());
    });
    layout.addWidget(games, {"Games to highlight", "live status", "game",
                             "controller", "category"});

    layout.addDescription(
        "Streamers to watch (shared with Notifications -> Selected Channels)");
    auto *watch = new EditableModelView(
        getApp()->getNotifications()->createModel(nullptr, Platform::Twitch));
    watch->setTitles({"Twitch streamers to watch"});
    watch->setValidationRegexp(twitchUserNameRegexp());
    watch->getTableView()->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    watch->setMaximumHeight(180);
    std::ignore = watch->addButtonPressed.connect([] {
        getApp()->getNotifications()->addChannelNotification("channel",
                                                             Platform::Twitch);
    });
    layout.addWidget(watch, {"streamers to watch", "watched", "live status",
                             "notifications", "closed tabs"});
    SettingWidget::checkbox(
        "Desktop notification when a watched streamer goes live",
        getSettings()->satorinoLiveDesktop)
        ->setTooltip(
            "Use the existing desktop notification backend for offline-to-live "
            "transitions only. No startup or reconnect alerts. "
            "Works only while satorino is running and subject to "
            "OS permissions. When off, the existing Notifications settings "
            "still control desktop alerts.")
        ->addKeywords({"live status", "watched", "toast", "desktop"})
        ->addTo(layout);
    SettingWidget::checkbox("Enable Discord webhook",
                            getSettings()->satorinoLiveDiscordEnabled)
        ->setTooltip(
            "Post watched offline-to-live transitions to Discord only while "
            "satorino is running. No app-start or reconnect messages.")
        ->addKeywords({"live status", "Discord", "webhook"})
        ->addTo(layout);

    auto *group = new QWidget;
    auto *box = new QVBoxLayout(group);
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(new QLabel("Discord webhook URL"));
    auto *endpoint = new QLineEdit;
    endpoint->setEchoMode(QLineEdit::Password);
    endpoint->setToolTip(
        "Anyone with this URL can post to that Discord channel. It is stored "
        "in your local settings file. Only HTTPS discord.com/discordapp.com "
        "webhook endpoints (including ptb/canary) are accepted.");
    getSettings()->satorinoLiveDiscordUrl.connect(
        [endpoint](const QString &value) {
            if (endpoint->text() != value)
                endpoint->setText(value);
        },
        this->managedConnections_);
    box->addWidget(endpoint);
    auto *show = new QCheckBox("Show URL");
    QObject::connect(show, &QCheckBox::toggled, endpoint,
                     [endpoint](bool visible) {
                         endpoint->setEchoMode(visible ? QLineEdit::Normal
                                                       : QLineEdit::Password);
                     });
    box->addWidget(show);
    auto *status = new QLabel;
    status->setWordWrap(true);
    auto save = [endpoint, status] {
        const auto value = endpoint->text().trimmed();
        if (!value.isEmpty() && !satorino::discordWebhookValid(value))
        {
            status->setText("Invalid Discord webhook URL. Use an HTTPS Discord "
                            "webhook endpoint.");
            return false;
        }
        getSettings()->satorinoLiveDiscordUrl.setValue(value);
        status->clear();
        return true;
    };
    QObject::connect(endpoint, &QLineEdit::editingFinished, this, [save] {
        save();
    });
    auto *test = new QPushButton("Send test message");
    box->addWidget(test);
    box->addWidget(status);
    QObject::connect(test, &QPushButton::clicked, this, [save, test, status] {
        if (!save())
            return;
        test->setEnabled(false);
        status->setText("Sending test message...");
        getApp()->getNotifications()->testSatorinoWebhook(
            [status = QPointer<QLabel>(status),
             test = QPointer<QPushButton>(test)](const QString &message) {
                if (status)
                    status->setText(message);
                if (test)
                    test->setEnabled(true);
            });
    });
    layout.addWidget(group, {"live status", "Discord", "webhook", "URL",
                             "test message", "notification"});
}

}  // namespace chatterino
