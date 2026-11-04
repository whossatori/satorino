// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/ChatterinoSetting.hpp"
#include "common/SignalVector.hpp"
#include "util/QCompareTransparent.hpp"

#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

namespace chatterino {

class Settings;
class Paths;
struct HelixStream;

class NotificationModel;
namespace satorino {
class LiveNotifications;
}  // namespace satorino

enum class Platform : uint8_t {
    Twitch,
};

class NotificationController final
{
public:
    NotificationController();

    void initialize();
    // satorino: observe existing open/closed-channel polls; never poll a second time.
    void observeSatorinoLive(const QString &login,
                             const std::optional<HelixStream> &stream,
                             bool initial);
    void syncSatorinoWatchList();
    void testSatorinoWebhook(std::function<void(QString)> result);

    bool isChannelNotified(const QString &channelName, Platform p) const;
    void updateChannelNotification(const QString &channelName, Platform p);
    void addChannelNotification(const QString &channelName, Platform p);
    void removeChannelNotification(const QString &channelName, Platform p);

    struct NotificationPayload {
        QString channelId;
        QString streamId;
        QString channelName;
        QString displayName;
        QString title;
        bool isInitialUpdate = false;
    };

    /// @brief Sends out notifications for a channel that has gone live
    ///
    /// Duplicate calls for the same stream have no effect
    void notifyTwitchChannelLive(const NotificationPayload &payload);

    void notifyTwitchChannelOffline(const QString &id) const;

    void playSound() const;

    NotificationModel *createModel(QObject *parent, Platform p);

private:
    std::shared_ptr<satorino::LiveNotifications>
        satorinoLive_;  // satorino: owned service
    void fetchFakeChannels();
    void removeFakeChannel(const QString &channelName);
    void updateFakeChannel(const QString &channelName,
                           const std::optional<HelixStream> &stream);

    struct FakeChannel {
        QString id;
        bool isLive = false;
    };

    std::map<QString, FakeChannel, QCompareCaseInsensitive> fakeChannels_;

    /// Maps channel IDs to their most recently notified stream IDs
    std::map<QString, QString> lastNotifiedStreamIds_;

    QTimer liveStatusTimer_;

    std::map<Platform, SignalVector<QString>> channelMap;

    ChatterinoSetting<std::vector<QString>> twitchSetting_ = {
        "/notifications/twitch"};
};

}  // namespace chatterino
