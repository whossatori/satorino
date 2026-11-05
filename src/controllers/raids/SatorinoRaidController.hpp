// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/eventsub/SubscriptionHandle.hpp"
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
    void receive(const QJsonObject &payload, qint64 timestampMs,
                 const QString &ownerId);

private:
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
    std::map<QString, eventsub::SubscriptionHandle> watches_;
    pajlada::Signals::SignalHolder settingsConnections_;
    pajlada::Signals::SignalHolder accountConnections_;
    boost::signals2::scoped_connection accountChangedConnection_;
};
}  // namespace satorino
}  // namespace chatterino
