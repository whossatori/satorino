// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoRaid.hpp"

#include <QChar>
#include <QJsonValue>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace chatterino::satorino {
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
std::optional<RaidEvent> parseRaidEvent(const QJsonObject &input,
                                        qint64 timestampMs, QString *error)
{
    const auto reject =
        [error](const QString &reason) -> std::optional<RaidEvent> {
        if (error)
            *error = reason;
        return std::nullopt;
    };
    if (error)
        error->clear();
    QJsonObject payload = input;
    if (input.contains("payload"))
    {
        const auto metadata = input.value("metadata").toObject();
        if (metadata.value("message_type").toString() != "notification" ||
            metadata.value("subscription_type").toString() != "channel.raid" ||
            metadata.value("subscription_version").toString() != "1")
            return reject("invalid notification metadata");
        payload = input.value("payload").toObject();
    }
    const auto subscription = payload.value("subscription").toObject();
    const auto event = payload.value("event").toObject();
    RaidEvent raid{
        event.value("from_broadcaster_user_id").toString(),
        event.value("to_broadcaster_user_id").toString(),
        event.value("to_broadcaster_user_login").toString().toLower(),
        event.value("to_broadcaster_user_name").toString(), timestampMs};
    const auto condition = subscription.value("condition").toObject();
    if (subscription.value("type").toString() != "channel.raid" ||
        subscription.value("version").toString() != "1")
        return reject("unsupported raid subscription type/version");
    if (condition.value("from_broadcaster_user_id").toString() !=
            raid.sourceId ||
        condition.contains("to_broadcaster_user_id"))
        return reject("incoming or mismatched source condition");
    if (!validRaidId(raid.sourceId) || !validRaidId(raid.targetId))
        return reject("invalid source/target ID");
    if (!validRaidLogin(raid.targetLogin))
        return reject("invalid target login");
    if (timestampMs <= 0)
        return reject("invalid notification timestamp");
    return raid;
}
RaidDecision RaidPolicy::decide(const RaidEvent &event,
                                const RaidContext &context)
{
    const auto ignore = [](const QString &reason) {
        return RaidDecision{RaidAction::Ignore, false, reason};
    };
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
            context.switchToTab && context.mainActive && !context.hasDraft,
            !context.switchToTab  ? "switch disabled"
            : !context.mainActive ? "main window inactive"
            : context.hasDraft    ? "current input has a draft"
                                  : QString{}};
}
QString raidSafeReason(QString reason, const QStringList &secrets)
{
    for (const auto &secret : secrets)
        if (!secret.isEmpty())
            reason.replace(secret, "[redacted]");
    reason.replace(
        QRegularExpression("(Bearer|OAuth)\\s+[^\\s]+",
                           QRegularExpression::CaseInsensitiveOption),
        "[redacted]");
    reason.replace(QRegularExpression("https?://[^\\s]+"), "[URL]");
    const auto brace = reason.indexOf('{');
    if (brace >= 0)
        reason = reason.left(brace) + "request rejected";
    return reason.simplified().left(160);
}
QString raidWatchStateText(const RaidSubscriptionStatus &status)
{
    switch (status.state)
    {
        case RaidWatchState::Inactive:
            return "not requested";
        case RaidWatchState::Waiting:
            return status.detail.isEmpty()
                       ? "waiting for connection"
                       : "retrying: " + raidSafeReason(status.detail);
        case RaidWatchState::Subscribing:
            return "subscribing";
        case RaidWatchState::Subscribed:
            return "subscribed";
        case RaidWatchState::Failed:
            return "failed: " + (status.detail.isEmpty()
                                     ? QString("request failed")
                                     : raidSafeReason(status.detail));
    }
    return "unknown";
}
QString raidStatusDetails(const RaidStatusContext &context)
{
    QStringList lines;
    for (const auto &watch : context.watches)
        lines.push_back(watch.login + ": " +
                        raidWatchStateText(watch.subscription) +
                        (watch.retrySeconds > 0
                             ? QString("; retry in %1s").arg(watch.retrySeconds)
                             : QString{}));
    if (!context.lastEvent.isEmpty())
        lines.push_back(context.lastEvent);
    return lines.join('\n');
}
QString raidStatusText(const RaidStatusContext &context)
{
    if (!context.enabled)
        return "Raid watch: off";
    if (!context.loggedIn)
        return "Raid watch: not logged in";
    if (!context.hasToken)
        return "Raid watch: account has no OAuth token";
    QString text;
    if (!context.eventSubAvailable)
        text = context.eventSubSetting
                   ? "Raid watch: EventSub disabled at startup; restart needed"
                   : "Raid watch: EventSub disabled in General > Advanced; "
                     "enable it and restart";
    else if (context.watches.isEmpty())
        text = context.waitingForIds ? "Raid watch: waiting for channel room ID"
                                     : "Raid watch: no open Twitch channels";
    else
    {
        QStringList names;
        int subscribed = 0;
        const RaidWatchStatus *failed = nullptr;
        for (const auto &watch : context.watches)
        {
            subscribed +=
                watch.subscription.state == RaidWatchState::Subscribed ? 1 : 0;
            if (!failed && watch.subscription.state == RaidWatchState::Failed)
                failed = &watch;
            if (names.size() < 3)
                names.push_back(watch.login + ": " +
                                raidWatchStateText(watch.subscription));
        }
        if (failed)
            text = "Raid watch: could not watch " + failed->login + ": " +
                   raidWatchStateText(failed->subscription) +
                   (failed->retrySeconds > 0
                        ? QString("; retry in %1s").arg(failed->retrySeconds)
                        : QString{});
        else if (subscribed == 0)
            text =
                "Raid watch: waiting for connection (" + names.join(", ") + ")";
        else
            text = QString("Raid watch: watching %1/%2 channels (%3%4)")
                       .arg(subscribed)
                       .arg(context.watches.size())
                       .arg(names.join(", "),
                            context.watches.size() > 3 ? ", ..." : "");
        if (!context.eventSubSetting)
            text += "; EventSub setting off, restart pending";
    }
    if (!context.lastEvent.isEmpty())
        text += " | " + context.lastEvent;
    else if (context.eventSubAvailable && !context.watches.isEmpty())
        text += "; no raid event received";
    return text;
}
int raidRetrySeconds(int failures)
{
    return std::min(60, 5 * (1 << std::clamp(failures, 0, 4)));
}
}  // namespace chatterino::satorino
