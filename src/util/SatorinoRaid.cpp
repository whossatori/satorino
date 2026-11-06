// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoRaid.hpp"

#include <QChar>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace chatterino::satorino {
namespace {
qint64 integer(const QJsonValue &value, qint64 fallback = 0)
{
    if (value.isString())
    {
        bool ok = false;
        const auto number = value.toString().toLongLong(&ok);
        return ok ? number : fallback;
    }
    const auto number =
        value.toDouble(std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < -9007199254740991.0 || number > 9007199254740991.0)
        return fallback;
    return static_cast<qint64>(number);
}
QJsonObject raidObject(const QJsonObject &payload)
{
    const auto raid = payload.value("raid").toObject();
    return raid.isEmpty()
               ? payload.value("data").toObject().value("raid").toObject()
               : raid;
}
QString field(const QJsonObject &object, const char *snake, const char *camel)
{
    return object.value(snake).toString(object.value(camel).toString());
}
}  // namespace
bool validRaidId(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 && id.front() != '0' &&
           std::all_of(id.begin(), id.end(), [](QChar c) {
               return c >= '0' && c <= '9';
           });
}
bool validRaidLogin(const QString &login)
{
    return !login.isEmpty() && login.size() <= 25 &&
           std::all_of(login.begin(), login.end(), [](QChar c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      c == '_';
           });
}
bool raidListeningEnabled(bool showBar, bool openTab)
{
    return showBar || openTab;
}
std::optional<QJsonObject> normaliseRaidFeed(const QString &topic,
                                             const QJsonObject &message)
{
    const auto type = message.value("type").toString();
    if (type != "raid_update_v2" && type != "raid_go_v2" &&
        type != "raid_cancel_v2")
        return std::nullopt;
    QJsonObject payload = message;
    payload.insert("topic", topic);
    const auto raid = raidObject(message);
    if (type == "raid_update_v2" && raid.isEmpty())
        return std::nullopt;
    payload.insert("raid", raid);
    return payload;
}
QString raidFeedId(const QJsonObject &payload)
{
    const auto raid = raidObject(payload);
    const auto data = payload.value("data").toObject();
    return raid.value("id").toString(
        payload.value("raid_id").toString(payload.value("raidId").toString(
            data.value("raid_id").toString(data.value("raidId").toString()))));
}
std::optional<RaidFeedMessage> parseRaidFeed(const QJsonObject &payload,
                                             qint64 receivedAtMs,
                                             QString *error)
{
    const auto reject =
        [error](const QString &reason) -> std::optional<RaidFeedMessage> {
        if (error)
            *error = reason;
        return std::nullopt;
    };
    if (error)
        error->clear();
    const auto topic = payload.value("topic").toString();
    const auto type = payload.value("type").toString();
    if (!topic.startsWith("raid.") || !validRaidId(topic.mid(5)))
        return reject("invalid raid topic/source");
    RaidFeedMessage message;
    message.sourceId = topic.mid(5);
    message.raidId = raidFeedId(payload);
    if (message.raidId.size() > 128)
        return reject("invalid raid ID");
    if (type == "raid_go_v2")
        message.kind = RaidFeedKind::Go;
    else if (type == "raid_cancel_v2")
        message.kind = RaidFeedKind::Cancel;
    else if (type != "raid_update_v2")
        return reject("unsupported raid feed type");
    const auto raid = raidObject(payload);
    const auto source = field(raid, "source_id", "sourceID");
    if (!source.isEmpty() && source != message.sourceId)
        return reject("source does not match raid topic");
    if (receivedAtMs <= 0)
        return reject("invalid feed receipt time");
    if (message.kind == RaidFeedKind::Cancel)
        return message;
    if (message.raidId.isEmpty() && message.kind == RaidFeedKind::Update)
        return reject("missing raid ID");
    message.targetIdHint = field(raid, "target_id", "targetID");
    message.targetLoginHint =
        field(raid, "target_login", "targetLogin").toLower();
    if ((!message.targetIdHint.isEmpty() &&
         !validRaidId(message.targetIdHint)) ||
        (!message.targetLoginHint.isEmpty() &&
         !validRaidLogin(message.targetLoginHint)))
        return reject("invalid raid target");
    if (!message.targetIdHint.isEmpty() && !message.targetLoginHint.isEmpty())
        message.event = RaidEvent{
            message.sourceId,
            message.targetIdHint,
            message.targetLoginHint,
            field(raid, "target_display_name", "targetDisplayName"),
            receivedAtMs,
            std::max(qint64(0), integer(raid.value("viewer_count").isUndefined()
                                            ? raid.value("viewerCount")
                                            : raid.value("viewer_count")))};
    if (message.kind == RaidFeedKind::Update && !message.event)
        return reject("countdown has no usable target");
    auto seconds = integer(raid.value("force_raid_now_seconds").isUndefined()
                               ? raid.value("forceRaidNowSeconds")
                               : raid.value("force_raid_now_seconds"),
                           90);
    if (seconds <= 0)
        seconds = 90;
    const bool safeDuration = seconds <= 3600;
    const auto created = integer(raid.value("raid_created_at").isUndefined()
                                     ? raid.value("raidCreatedAt")
                                     : raid.value("raid_created_at"));
    const auto nowSeconds = receivedAtMs / 1000;
    message.absoluteDeadline = created > 0 && created >= nowSeconds - 3600 &&
                               created <= nowSeconds + 30;
    message.deadlineMs =
        safeDuration
            ? (message.absoluteDeadline ? created * 1000 : receivedAtMs) +
                  seconds * 1000
            : 0;
    return message;
}
RaidFeedResult RaidFeedTracker::observe(const QJsonObject &payload,
                                        qint64 receivedAtMs)
{
    QString error;
    const auto message = parseRaidFeed(payload, receivedAtMs, &error);
    if (!message)
        return {RaidFeedAction::Ignore, {}, {}, {}, error};
    std::erase_if(this->cancelled_, [receivedAtMs](const auto &entry) {
        return receivedAtMs - entry.second >= RAID_DEDUPE_MS;
    });
    auto it = this->pending_.find(message->sourceId);
    if (message->kind == RaidFeedKind::Cancel)
    {
        const auto cancelledId =
            message->raidId.isEmpty() && it != this->pending_.end()
                ? it->second.id
                : message->raidId;
        if (!cancelledId.isEmpty())
        {
            if (this->cancelled_.size() >= 128)
            {
                const auto oldest = std::min_element(
                    this->cancelled_.begin(), this->cancelled_.end(),
                    [](const auto &a, const auto &b) {
                        return a.second < b.second;
                    });
                this->cancelled_.erase(oldest);
            }
            this->cancelled_[{message->sourceId, cancelledId}] = receivedAtMs;
        }
        if (it != this->pending_.end() && !message->raidId.isEmpty() &&
            it->second.id != message->raidId)
            return {RaidFeedAction::Ignore,
                    message->sourceId,
                    message->raidId,
                    {},
                    "cancel belongs to an older raid"};
        if (it != this->pending_.end() &&
            it->second.state == RaidFeedAction::Fire)
            return {RaidFeedAction::Ignore,
                    message->sourceId,
                    message->raidId,
                    {},
                    "raid already fired"};
        if (it == this->pending_.end() && message->raidId.isEmpty())
            return {RaidFeedAction::Ignore,
                    message->sourceId,
                    {},
                    {},
                    "no active raid to cancel"};
        auto &pending = this->pending_[message->sourceId];
        if (!message->raidId.isEmpty())
            pending.id = message->raidId;
        pending.state = RaidFeedAction::Cancelled;
        pending.deadline = 0;
        return {RaidFeedAction::Cancelled, message->sourceId, pending.id,
                pending.event, "cancelled"};
    }
    const auto id = message->raidId.isEmpty() && it != this->pending_.end()
                        ? it->second.id
                        : message->raidId;
    if (id.isEmpty())
        return {RaidFeedAction::Ignore,
                message->sourceId,
                {},
                {},
                "missing raid ID"};
    if (this->cancelled_.contains({message->sourceId, id}))
        return {RaidFeedAction::Ignore,
                message->sourceId,
                id,
                {},
                "raid was cancelled"};
    const bool same = it != this->pending_.end() && it->second.id == id;
    if (same && (it->second.state == RaidFeedAction::Cancelled ||
                 it->second.state == RaidFeedAction::Fire))
        return {RaidFeedAction::Ignore, message->sourceId, id,
                message->event ? message->event : it->second.event,
                it->second.state == RaidFeedAction::Cancelled
                    ? "raid was cancelled"
                    : "duplicate completed raid"};
    auto event = message->event;
    if (!event && same && it->second.event)
    {
        event = it->second.event;
        if ((!message->targetIdHint.isEmpty() &&
             message->targetIdHint != event->targetId) ||
            (!message->targetLoginHint.isEmpty() &&
             message->targetLoginHint != event->targetLogin))
        {
            it->second.deadline = 0;
            it->second.state = RaidFeedAction::Ignore;
            return {RaidFeedAction::Ignore,
                    message->sourceId,
                    id,
                    {},
                    "go target does not match countdown"};
        }
        event->timestampMs = receivedAtMs;
    }
    if (!event)
        return {RaidFeedAction::Ignore,
                message->sourceId,
                id,
                {},
                "go has no usable target/countdown"};
    auto &pending = this->pending_[message->sourceId];
    const auto previousDeadline = same ? pending.deadline : 0;
    pending.id = id;
    pending.event = event;
    if (message->kind == RaidFeedKind::Go)
    {
        pending.state = RaidFeedAction::Fire;
        pending.deadline = 0;
        return {RaidFeedAction::Fire, message->sourceId, id, event, "go"};
    }
    pending.state = RaidFeedAction::Wait;
    pending.deadline =
        same && !message->absoluteDeadline && previousDeadline > 0
            ? previousDeadline
            : message->deadlineMs;
    return {RaidFeedAction::Wait, message->sourceId, id, event, "countdown"};
}
QVector<RaidFeedResult> RaidFeedTracker::expire(qint64 nowMs)
{
    QVector<RaidFeedResult> results;
    for (auto &[source, pending] : this->pending_)
        if (pending.state == RaidFeedAction::Wait && pending.event &&
            pending.deadline > 0 && pending.deadline <= nowMs)
        {
            pending.state = RaidFeedAction::Fire;
            pending.deadline = 0;
            pending.event->timestampMs = nowMs;
            results.push_back({RaidFeedAction::Fire, source, pending.id,
                               pending.event, "countdown end"});
        }
    return results;
}
std::optional<qint64> RaidFeedTracker::nextDeadline() const
{
    std::optional<qint64> next;
    for (const auto &[source, pending] : this->pending_)
        if (pending.state == RaidFeedAction::Wait && pending.deadline > 0 &&
            (!next || pending.deadline < *next))
            next = pending.deadline;
    return next;
}
void RaidFeedTracker::retain(const std::set<QString> &sources)
{
    std::erase_if(this->pending_, [&sources](const auto &entry) {
        return !sources.contains(entry.first);
    });
    std::erase_if(this->cancelled_, [&sources](const auto &entry) {
        return !sources.contains(entry.first.first);
    });
}
bool raidHasUnsentDraft(const QString &current, const QString &submitting)
{
    return !current.isEmpty() &&
           (submitting.isEmpty() || current != submitting);
}
bool raidCanSwitch(bool applicationActive, bool hasDraft)
{
    return applicationActive && !hasDraft;
}
RaidSelection raidSelection(bool switchOption, bool applicationActive,
                            bool hasDraft)
{
    return !switchOption || hasDraft ? RaidSelection::None
           : applicationActive       ? RaidSelection::Foreground
                                     : RaidSelection::Background;
}
RaidScopeDecision raidScopeDecision(bool sourceSelected,
                                    RaidSelection existingSelection)
{
    return {sourceSelected,
            sourceSelected ? existingSelection : RaidSelection::None};
}
bool raidSourceInSelectedTabs(const QString &source,
                              const std::set<QString> &selectedSources)
{
    return validRaidId(source) && selectedSources.contains(source);
}
QString raidSelectionStatus(RaidSelection selection, bool switchOption,
                            bool hasDraft)
{
    if (selection == RaidSelection::Foreground)
        return "switched";
    if (selection == RaidSelection::Background)
        return "selected in background (application inactive)";
    return !switchOption ? "not switched: switch option off"
           : hasDraft    ? "not switched: draft in current input"
                         : "not switched: target closed or setting changed";
}
RaidDecision RaidPolicy::decide(const RaidEvent &event,
                                const RaidContext &context)
{
    const auto ignore = [](const QString &reason) {
        return RaidDecision{RaidAction::Ignore, false, reason};
    };
    const auto scope =
        raidScopeDecision(context.sourceSelected,
                          raidSelection(context.switchToTab, context.mainActive,
                                        context.hasDraft));
    // satorino: reject before dedupe pruning/insertion or cap bookkeeping.
    if (!scope.act)
        return ignore("source is not displayed in a selected tab");
    if (!context.enabled)
        return ignore("setting off");
    if (!context.loggedIn)
        return ignore("not logged in");
    if (!context.twitch)
        return ignore("not a Twitch channel");
    if (!context.sourceOpen)
        return ignore("source is no longer open");
    if (context.targetBlocked)
        return ignore("target is blocked");
    if (!validRaidId(event.sourceId) || !validRaidId(event.targetId) ||
        !validRaidLogin(event.targetLogin))
        return ignore("invalid source/target");
    if (event.sourceId == event.targetId)
        return ignore("target is the source");
    if (event.timestampMs <= 0)
        return ignore("invalid notification timestamp");
    const auto arrival = context.receivedAtUtcMs > 0 ? context.receivedAtUtcMs
                                                     : event.timestampMs;
    if (arrival < context.enabledAtUtcMs)
        return ignore("received before watch started");
    if (event.timestampMs > context.nowUtcMs + 30000)
        return ignore(
            "notification is more than 30 seconds in the future (check clock)");
    if (event.timestampMs < context.nowUtcMs - RAID_DEDUPE_MS)
        return ignore("notification is older than three minutes");
    while (!this->seen_.empty() &&
           context.nowMs - this->seen_.front().at >= RAID_DEDUPE_MS)
        this->seen_.pop_front();
    while (!this->opened_.empty() &&
           context.nowMs - this->opened_.front() >= RAID_CAP_WINDOW_MS)
        this->opened_.pop_front();
    if (std::any_of(this->seen_.begin(), this->seen_.end(),
                    [&event](const Seen &seen) {
                        return seen.source == event.sourceId &&
                               seen.target == event.targetId;
                    }))
        return ignore("duplicate source/target within three minutes");
    if (!context.targetExists &&
        ((!this->opened_.empty() &&
          context.nowMs - this->opened_.back() < RAID_NEW_TAB_GAP_MS) ||
         this->opened_.size() >= 5))
        return ignore("new-tab rate limit (one/15s, five/10min)");
    if (this->seen_.size() >= 128)
        this->seen_.pop_front();
    this->seen_.push_back({event.sourceId, event.targetId, context.nowMs});
    if (!context.targetExists)
        this->opened_.push_back(context.nowMs);
    return {context.targetExists ? RaidAction::SelectExisting
                                 : RaidAction::OpenAndJoin,
            context.switchToTab && !context.hasDraft,
            !context.switchToTab ? "switch option off"
            : context.hasDraft   ? "draft in current input"
                                 : QString{},
            scope.selection};
}
QString raidStatusText(const RaidStatusContext &context)
{
    if (!context.enabled || !context.loggedIn)
        return "Raid watch off";
    const int count = std::max(0, context.listening);
    return count == 1 ? QString("Raid watch on 1 channel")
                      : QString("Raid watch on %1 channels").arg(count);
}
QString raidStatusDetails(const RaidStatusContext &context)
{
    return QString("Shared raid status feed (raid bar): %1 open channels "
                   "requested; %2 waiting for a room ID. Trigger: raid_go_v2; "
                   "cancellable one-shot countdown end fallback. Existing "
                   "PubSub connection/acknowledgement handling applies; this "
                   "count is not a live delivery guarantee.")
        .arg(context.listening)
        .arg(context.waitingForIds);
}
}  // namespace chatterino::satorino
