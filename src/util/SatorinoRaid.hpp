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
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace chatterino::satorino {
inline constexpr qint64 RAID_DEDUPE_MS = 180000;
inline constexpr qint64 RAID_NEW_TAB_GAP_MS = 15000;
inline constexpr qint64 RAID_CAP_WINDOW_MS = 600000;
struct RaidEvent {
    QString sourceId;
    QString targetId;
    QString targetLogin;
    QString targetName;
    qint64 timestampMs = 0;  // Local feed receipt / synthetic-go time, UTC.
    qint64 viewerCount = 0;
};
bool validRaidId(const QString &id);
bool validRaidLogin(const QString &login);
bool raidListeningEnabled(bool showBar, bool openTab);
// An exact, currently executing submission is not an unsent draft. Never infer from /command text.
bool raidHasUnsentDraft(const QString &current, const QString &submitting = {});
bool raidCanSwitch(bool applicationActive, bool hasDraft);
std::optional<QJsonObject> normaliseRaidFeed(const QString &topic,
                                             const QJsonObject &message);
QString raidFeedId(const QJsonObject &payload);
enum class RaidFeedKind { Update, Go, Cancel };
struct RaidFeedMessage {
    RaidFeedKind kind = RaidFeedKind::Update;
    QString sourceId;
    QString raidId;
    QString targetIdHint;
    QString targetLoginHint;
    std::optional<RaidEvent> event;
    qint64 deadlineMs = 0;
    bool absoluteDeadline = false;
};
std::optional<RaidFeedMessage> parseRaidFeed(const QJsonObject &payload,
                                             qint64 receivedAtMs,
                                             QString *error = nullptr);
std::optional<RaidEvent> parseRaidEvent(const QJsonObject &payload,
                                        qint64 receivedAtMs,
                                        QString *error = nullptr);
enum class RaidFeedAction { Wait, Fire, Cancelled, Ignore };
struct RaidFeedResult {
    RaidFeedAction action = RaidFeedAction::Ignore;
    QString sourceId;
    QString raidId;
    std::optional<RaidEvent> event;
    QString reason;
};
class RaidFeedTracker
{
public:
    RaidFeedResult observe(const QJsonObject &payload, qint64 receivedAtMs);
    QVector<RaidFeedResult> expire(qint64 nowMs);
    std::optional<qint64> nextDeadline() const;
    void retain(const std::set<QString> &sources);

private:
    struct Pending {
        QString id;
        std::optional<RaidEvent> event;
        qint64 deadline = 0;
        RaidFeedAction state = RaidFeedAction::Wait;
    };
    std::map<QString, Pending> pending_;
    std::map<std::pair<QString, QString>, qint64>
        cancelled_;  // Short tombstones for reordered cancels
};
struct RaidContext {
    bool enabled = false;
    bool switchToTab = true;
    bool targetExists = false;
    bool targetBlocked = false;
    bool loggedIn = false;
    bool twitch = true;
    bool sourceOpen = false;
    bool mainActive =
        false;  // Application foreground, including its popups/secondary windows.
    bool hasDraft = false;
    qint64 nowMs = 0;
    qint64 nowUtcMs = 0;
    qint64 enabledAtUtcMs = 0;
    qint64 receivedAtUtcMs = 0;
    bool sourceSelected = false;  // satorino: arrival-time selected-tab scope
};
enum class RaidAction { Ignore, OpenAndJoin, SelectExisting };
enum class RaidSelection { None, Background, Foreground };
// satorino: tab scope controls the entire action, independently of focus/draft selection.
struct RaidScopeDecision {
    bool act;
    RaidSelection selection;
};
RaidScopeDecision raidScopeDecision(bool sourceSelected,
                                    RaidSelection existingSelection);
bool raidSourceInSelectedTabs(const QString &source,
                              const std::set<QString> &selectedSources);
RaidSelection raidSelection(bool switchOption, bool applicationActive,
                            bool hasDraft);
QString raidSelectionStatus(RaidSelection selection, bool switchOption,
                            bool hasDraft);
struct RaidDecision {
    RaidAction action = RaidAction::Ignore;
    bool switchToTab = false;
    QString reason;
    RaidSelection selection = RaidSelection::None;
};
struct RaidStatusContext {
    bool enabled = false;
    bool loggedIn = false;
    int listening = 0;
    int waitingForIds = 0;
    QString lastEvent;
};
QString raidStatusText(const RaidStatusContext &context);
QString raidStatusDetails(const RaidStatusContext &context);
struct RaidTestCommand {
    QString login;
    QString id;
};
std::optional<RaidTestCommand> parseRaidTestCommand(const QStringList &words,
                                                    QString *error = nullptr);
QString raidTestKnownId(const RaidTestCommand &command, const QString &localId);
enum class RaidLookupStatus { Found, NotFound, Error, Timeout };
struct RaidLookupResult {
    bool usable = false;
    QString id;
    QString reason;
};
RaidLookupResult raidLookupResult(const QString &login, RaidLookupStatus status,
                                  const QString &returnedId = {},
                                  const QString &returnedLogin = {});
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
