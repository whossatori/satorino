// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoSuspiciousUsers.hpp"
#include "util/SatorinoFlags.hpp"
#include <algorithm>

namespace chatterino::satorino {
bool flagControlVisible(bool realTwitch, bool canModerate, FlagTargetRole role)
{
    return realTwitch && canModerate && role == FlagTargetRole::Regular;
}
QString suspiciousControlLabel(SuspiciousState state)
{
    switch (state)
    {
        case SuspiciousState::Monitored:
            return "Monitored";
        case SuspiciousState::Restricted:
            return "Restricted";
        default:
            return "Flag user";
    }
}
std::vector<SuspiciousAction> suspiciousMenuActions(SuspiciousState state)
{
    switch (state)
    {
        case SuspiciousState::None:
            return {SuspiciousAction::Monitor, SuspiciousAction::Restrict};
        case SuspiciousState::Monitored:
            return {SuspiciousAction::Restrict, SuspiciousAction::Remove};
        case SuspiciousState::Restricted:
            // Keep only removal; no unverified restricted -> monitored shortcut.
            return {SuspiciousAction::Remove};
        case SuspiciousState::Unknown:
            return {SuspiciousAction::Monitor, SuspiciousAction::Restrict,
                    SuspiciousAction::Remove};
    }
    return {};
}
QString suspiciousActionLabel(SuspiciousAction action, SuspiciousState state,
                              const QString &name)
{
    if (action == SuspiciousAction::Monitor)
        return "Monitor " + name;
    if (action == SuspiciousAction::Restrict)
        return "Restrict " + name;
    if (state == SuspiciousState::Monitored)
        return "Stop monitoring";
    if (state == SuspiciousState::Restricted)
        return "Stop restricting";
    return "Remove flag";
}
SuspiciousState SuspiciousUserCache::state(const QString &room,
                                           const QString &user) const
{
    const auto it = this->entries_.find({room, user});
    return it == this->entries_.end() ? SuspiciousState::Unknown
                                      : it->second.state;
}
bool SuspiciousUserCache::observe(const QString &room, const QString &user,
                                  SuspiciousState state, qint64 atMs)
{
    if (!flagTarget(room, user) || state == SuspiciousState::Unknown ||
        atMs <= 0)
        return false;
    const Key key{room, user};
    const auto old = this->entries_.find(key);
    if (old != this->entries_.end() &&
        (atMs < old->second.atMs ||
         (atMs == old->second.atMs && state == old->second.state)))
        return false;
    this->entries_[key] = {state, atMs, ++this->serial_};
    // Eviction uses accepted observation order; reading does not refresh age.
    const auto evictOldest = [this, &room](bool sameRoom) {
        auto oldest = this->entries_.end();
        for (auto it = this->entries_.begin(); it != this->entries_.end(); ++it)
            if ((!sameRoom || it->first.first == room) &&
                (oldest == this->entries_.end() ||
                 it->second.version < oldest->second.version))
                oldest = it;
        if (oldest != this->entries_.end())
            this->entries_.erase(oldest);
    };
    const auto roomCount =
        std::count_if(this->entries_.begin(), this->entries_.end(),
                      [&room](const auto &entry) {
                          return entry.first.first == room;
                      });
    if (static_cast<std::size_t>(roomCount) > PER_ROOM_CAP)
        evictOldest(true);
    if (this->entries_.size() > TOTAL_CAP)
        evictOldest(false);
    return true;
}
std::size_t SuspiciousUserCache::size() const
{
    return this->entries_.size();
}
bool SuspiciousUserCache::busy(const QString &room, const QString &user) const
{
    return this->pending_.contains({room, user});
}
std::optional<std::uint64_t> SuspiciousUserCache::begin(const QString &room,
                                                        const QString &user)
{
    const Key key{room, user};
    if (!flagTarget(room, user) || this->pending_.contains(key) ||
        this->pending_.size() >= PENDING_CAP)
        return std::nullopt;
    const auto it = this->entries_.find(key);
    const auto version =
        it == this->entries_.end() ? std::uint64_t{0} : it->second.version;
    const auto token = ++this->serial_;
    this->pending_[key] = {token, version};
    return token;
}
bool SuspiciousUserCache::complete(const QString &room, const QString &user,
                                   std::uint64_t token,
                                   std::optional<SuspiciousState> success,
                                   qint64 atMs)
{
    const Key key{room, user};
    const auto pending = this->pending_.find(key);
    if (pending == this->pending_.end() || pending->second.token != token)
        return false;
    const auto version = pending->second.version;
    this->pending_.erase(pending);
    const auto current = this->entries_.find(key);
    // A feed observation accepted during the request wins over its late callback.
    if (success &&
        (current == this->entries_.end() || current->second.version <= version))
    {
        // A successful API result is still authoritative if the server clock is ahead.
        const auto resultAt = current == this->entries_.end()
                                  ? atMs
                                  : std::max(atMs, current->second.atMs);
        this->observe(room, user, *success, resultAt);
    }
    return true;
}
}  // namespace chatterino::satorino
