// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"

#include "Application.hpp"
#include "controllers/notifications/NotificationController.hpp"
#include "controllers/notifications/NotificationModel.hpp"
#include "controllers/trains/SatorinoTrainProducer.hpp"
#include "singletons/Settings.hpp"
#include "util/SatorinoGames.hpp"
#include "util/SatorinoLive.hpp"
#include "util/SatorinoTrain.hpp"
#include "util/Twitch.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include "widgets/settingspages/SatorinoGamePicker.hpp"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>  // satorino: direct widget dependencies, independent of PCH
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstddef>
#include <tuple>  // satorino: std::ignore

namespace chatterino {

void SatorinoPage::addLiveStatus(GeneralPageView &layout)
{
    layout.addTitle("Live status");
    layout.addDescription("Notifications work only while satorino is running, "
                          "including for streamers without an open tab.");
    layout.addWidget(new SatorinoGamePicker(this),
                     {"Games to highlight", "Search games", "Selected games",
                      "game", "category", "colour", "live"});

    SettingWidget::checkbox("Show highlighted games as a star instead of a dot",
                            getSettings()->satorinoLiveGameStar)
        ->setTooltip(
            "Matching live games use a five-point star; turn this off "
            "for a full-size coloured dot. "
            "Both use the selected game's colour swatch. Other live "
            "tabs keep the stock dot. The chosen colour is used exactly.")
        ->addKeywords({"star", "dot", "icon", "game", "live status"})
        ->addTo(layout);

    // satorino: train styles are ready; the status reflects the actual producer availability.
    layout.addSubtitle("Hype train indicators");
    SettingWidget::checkbox("Show hype train indicators",
                            getSettings()->satorinoLiveTrainEnabled)
        ->setTooltip("Trains take priority over game highlights and the live "
                     "dot. Golden Kappa uses a star; other types use a dot. "
                     "Game star settings are independent.")
        ->addKeywords({"hype train", "golden kappa", "colour", "star"})
        ->addTo(layout);
    auto *trainGroup = new QWidget;
    auto *trainTable = new QGridLayout(trainGroup);
    trainTable->setContentsMargins(0, 0, 0, 0);
    trainTable->setHorizontalSpacing(12);
    trainTable->setColumnStretch(2, 1);
    auto *trainStatus = new QLabel;
    trainStatus->setTextFormat(Qt::PlainText);
    trainStatus->setWordWrap(true);
    trainStatus->setToolTip(
        "No verified viewer feed for current train types is connected. These "
        "colours are saved for when train data is available.");
    auto *swatches = new QWidget(trainGroup);
    auto *rows = new QGridLayout(swatches);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setHorizontalSpacing(12);
    rows->setColumnStretch(2, 1);
    rows->addWidget(new QLabel("Train type"), 0, 0);
    rows->addWidget(new QLabel("Colour"), 0, 1);
    std::array<QPushButton *, satorino::TRAIN_TYPE_COUNT> trainButtons{};
    for (std::size_t i = 0; i < satorino::TRAIN_TYPE_COUNT; ++i)
    {
        const auto type = static_cast<satorino::TrainType>(i);
        const auto name = satorino::trainTypeName(type);
        const auto row = static_cast<int>(i) + 1;
        rows->addWidget(new QLabel(name), row, 0);
        auto *button = new QPushButton;
        button->setFixedSize(28, 26);
        button->setAccessibleName(name + " train colour");
        button->setToolTip(name + (type == satorino::TrainType::GoldenKappa
                                       ? " star colour"
                                       : " dot colour"));
        rows->addWidget(button, row, 1);
        trainButtons[i] = button;
        QObject::connect(button, &QPushButton::clicked, this, [this, i, name] {
            const QPointer<SatorinoPage> guard(this);
            const auto chosen = QColorDialog::getColor(
                getSettings()->satorinoLiveTrainColours.getValue().values[i],
                this, name + " train colour", QColorDialog::ShowAlphaChannel);
            if (guard && chosen.isValid())
            {
                auto colours =
                    getSettings()->satorinoLiveTrainColours.getValue();
                colours.values[i] = chosen;
                getSettings()->satorinoLiveTrainColours.setValue(colours);
            }
        });
    }
    trainTable->addWidget(trainStatus, 0, 0, 1, 3);
    trainTable->addWidget(swatches, 1, 0, 1, 3);
    getSettings()->satorinoLiveTrainEnabled.connect(
        [swatches, trainStatus](bool enabled) {
            swatches->setEnabled(enabled);
            trainStatus->setText(satorino::trainDataStatus(
                enabled, satorino::trainProducer().available()));
        },
        this->managedConnections_);
    getSettings()->satorinoLiveTrainColours.connect(
        [trainButtons](const satorino::TrainColours &colours) {
            for (std::size_t i = 0; i < trainButtons.size(); ++i)
            {
                const auto colour = colours.values[i];
                // rgba() preserves the alpha in the preview as well as storage and tab painting.
                trainButtons[i]->setStyleSheet(
                    QString("background-color: rgba(%1, %2, %3, %4); border: "
                            "1px solid %5;")
                        .arg(colour.red())
                        .arg(colour.green())
                        .arg(colour.blue())
                        .arg(colour.alpha())
                        .arg(satorino::dotOutline(colour).name()));
            }
        },
        this->managedConnections_);
    layout.addWidget(trainGroup,
                     {"hype train", "train type", "golden kappa", "treasure",
                      "shared", "mythic", "community", "colour"});

    layout.addSubtitle("Live notifications");
    layout.addDescription("Shared with Notifications -> Selected Channels.");
    auto *watch = new EditableModelView(
        getApp()->getNotifications()->createModel(nullptr, Platform::Twitch));
    watch->setTitles({"Streamer"});
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
