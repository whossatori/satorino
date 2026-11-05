// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoRaid.hpp"

#include <QChar>
#include <QJsonValue>

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
std::optional<RaidEvent> parseRaidEvent(const QJsonObject &payload,
                                        qint64 timestampMs)
{
    const auto subscription = payload.value("subscription").toObject();
    const auto event = payload.value("event").toObject();
    RaidEvent raid{
        event.value("from_broadcaster_user_id").toString(),
        event.value("to_broadcaster_user_id").toString(),
        event.value("to_broadcaster_user_login").toString().toLower(),
        event.value("to_broadcaster_user_name").toString(), timestampMs};
    const auto condition = subscription.value("condition").toObject();
    if (subscription.value("type").toString() != "channel.raid" ||
        subscription.value("version").toString() != "1" ||
        condition.value("from_broadcaster_user_id").toString() !=
            raid.sourceId ||
        condition.contains("to_broadcaster_user_id") ||
        !validRaidId(raid.sourceId) || !validRaidId(raid.targetId) ||
        !validRaidLogin(raid.targetLogin) || timestampMs <= 0)
        return std::nullopt;
    return raid;
}
RaidDecision RaidPolicy::decide(const RaidEvent &event,
                                const RaidContext &context)
{
    if (!context.enabled || !context.loggedIn || !context.twitch ||
        !context.sourceOpen || context.targetBlocked ||
        !validRaidId(event.sourceId) || !validRaidId(event.targetId) ||
        !validRaidLogin(event.targetLogin) ||
        event.sourceId == event.targetId || event.timestampMs <= 0 ||
        event.timestampMs < context.enabledAtUtcMs ||
        event.timestampMs > context.nowUtcMs + 30000 ||
        event.timestampMs < context.nowUtcMs - RAID_DEDUPE_MS)
        return {};
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
        return {};
    if (!context.targetExists &&
        ((!this->opened_.empty() &&
          context.nowMs - this->opened_.back() < RAID_NEW_TAB_GAP_MS) ||
         this->opened_.size() >= 5))
        return {};
    if (this->seen_.size() >= 128)
        this->seen_.pop_front();
    this->seen_.push_back({event.sourceId, event.targetId, context.nowMs});
    if (!context.targetExists)
        this->opened_.push_back(context.nowMs);
    return {context.targetExists ? RaidAction::SelectExisting
                                 : RaidAction::OpenAndJoin,
            context.switchToTab && context.mainActive && !context.hasDraft};
}
}  // namespace chatterino::satorino
