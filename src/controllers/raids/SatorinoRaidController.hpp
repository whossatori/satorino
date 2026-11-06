// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoRaid.hpp"

#include <boost/signals2/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>

namespace chatterino {
class WindowManager;
namespace satorino {
class RaidController : public QObject
{
public:
    explicit RaidController(WindowManager &windows);
    QString receive(const QJsonObject &payload, qint64 receivedAtMs);
    RaidStatusContext statusContext() const;
private:
    QString queueAction(const RaidEvent &event, qint64 receivedAtMs,
                        const QString &trigger);
    QString act(const RaidEvent &event, qint64 receivedAtMs,
                const QString &trigger);
    QString record(const QString &sourceId,
                   const std::optional<RaidEvent> &event,
                   const QString &result);
    void configure();
    void accountChanged();
    void stop();
    void scheduleDeadline();
    void deadlineExpired();
    WindowManager &windows_;
    QTimer deadlineTimer_;  // One-shot fallback only, never a polling timer.
    QElapsedTimer clock_;
    qint64 enabledAtUtcMs_ = 0;
    std::uint64_t generation_ = 0;
    QString ownerId_;
    QString lastEvent_;
    QString lastStatus_;
    RaidPolicy policy_;
    RaidFeedTracker feed_;
    pajlada::Signals::SignalHolder settingsConnections_;
    pajlada::Signals::SignalHolder accountConnections_;
    pajlada::Signals::SignalHolder feedConnection_;
    boost::signals2::scoped_connection accountChangedConnection_;
};
}  // namespace satorino
}  // namespace chatterino
