// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/eventsub/SubscriptionHandle.hpp"
#include "providers/twitch/eventsub/SubscriptionRequest.hpp"
#include "common/Channel.hpp"
#include "util/SatorinoRaid.hpp"

#include <boost/signals2/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <map>

namespace chatterino {
class WindowManager;
namespace satorino {
class RaidController : public QObject
{
public:
    explicit RaidController(WindowManager &windows);
    QString receive(const QJsonObject &payload, qint64 timestampMs,
                    const QString &ownerId, qint64 receivedAtMs = 0);
    QString simulate(const ChannelPtr &source, const QString &targetLogin,
                     const QString &targetId);
    RaidStatusContext statusContext() const;

private:
    QString receiveImpl(const QJsonObject &payload, qint64 timestampMs,
                        const QString &ownerId, qint64 receivedAtMs,
                        bool synthetic);
    void publishStatus();
    void configure();
    void accountChanged();
    void sync();
    void stop();
    WindowManager &windows_;
    QTimer timer_;
    QElapsedTimer clock_;
    qint64 enabledAtUtcMs_ = 0;
    QString ownerId_;
    RaidPolicy policy_;
    struct Watch {
        eventsub::SubscriptionRequest request;
        eventsub::SubscriptionHandle handle;
        QString login;
        RaidSubscriptionStatus last;
        int failures = 0;
        qint64 retryAt = 0;
        qint64 startedAt = 0;
    };
    QString token_;  // In-memory comparison only; never shown/logged.
    QString client_;
    QString lastEvent_;
    QString lastStatus_;
    bool waitingForIds_ = false;
    std::map<QString, Watch> watches_;
    pajlada::Signals::SignalHolder settingsConnections_;
    pajlada::Signals::SignalHolder accountConnections_;
    boost::signals2::scoped_connection accountChangedConnection_;
};
}  // namespace satorino
}  // namespace chatterino
