// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <deque>
#include <optional>

namespace chatterino::satorino {
inline constexpr qint64 RAID_DEDUPE_MS = 180000;
inline constexpr qint64 RAID_NEW_TAB_GAP_MS = 15000;
inline constexpr qint64 RAID_CAP_WINDOW_MS = 600000;
inline constexpr int RAID_WATCH_LIMIT = 10;
struct RaidEvent {
    QString sourceId;
    QString targetId;
    QString targetLogin;
    QString targetName;
    qint64 timestampMs = 0;  // EventSub notification metadata, UTC.
};
bool validRaidId(const QString &id);
bool validRaidLogin(const QString &login);
std::optional<RaidEvent> parseRaidEvent(const QJsonObject &payload,
                                        qint64 timestampMs,
                                        QString *error = nullptr);
struct RaidContext {
    bool enabled = false;
    bool switchToTab = true;
    bool targetExists = false;
    bool targetBlocked = false;
    bool loggedIn = false;
    bool twitch = true;
    bool sourceOpen = false;
    bool mainActive = false;
    bool hasDraft = false;
    qint64 nowMs = 0;  // Monotonic time for caps/dedupe.
    qint64 nowUtcMs = 0;
    qint64 enabledAtUtcMs = 0;
    qint64 receivedAtUtcMs =
        0;  // Local arrival clock avoids server skew at the enable boundary.
};
enum class RaidAction { Ignore, OpenAndJoin, SelectExisting };
struct RaidDecision {
    RaidAction action = RaidAction::Ignore;
    bool switchToTab = false;
    QString reason;
};
enum class RaidWatchState {
    Inactive,
    Waiting,
    Subscribing,
    Subscribed,
    Failed
};
struct RaidSubscriptionStatus {
    RaidWatchState state = RaidWatchState::Inactive;
    QString detail;
};
struct RaidWatchStatus {
    QString login;
    RaidSubscriptionStatus subscription;
    int retrySeconds = 0;
};
struct RaidStatusContext {
    bool enabled = false;
    bool loggedIn = false;
    bool hasToken = false;
    bool eventSubAvailable = false;
    bool eventSubSetting = false;
    bool waitingForIds = false;
    QVector<RaidWatchStatus> watches;
    QString lastEvent;
};
QString raidSafeReason(QString reason, const QStringList &secrets = {});
QString raidWatchStateText(const RaidSubscriptionStatus &status);
QString raidStatusText(const RaidStatusContext &context);
QString raidStatusDetails(const RaidStatusContext &context);
int raidRetrySeconds(int failures);
class RaidPolicy
{
public:
    RaidDecision decide(const RaidEvent &event, const RaidContext &context);

private:
    struct Seen {
        QString source;
        QString target;
        qint64 at;
    };
    std::deque<Seen> seen_;
    std::deque<qint64> opened_;
};
}  // namespace chatterino::satorino
