// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include <QString>
#include <QtGlobal>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace chatterino::satorino {
enum class SuspiciousState { Unknown, None, Monitored, Restricted };
enum class SuspiciousAction { Monitor, Restrict, Remove };
enum class FlagTargetRole { Unknown, Regular, Self, Broadcaster, Moderator };
bool flagControlVisible(bool realTwitch, bool canModerate, FlagTargetRole role);
QString suspiciousControlLabel(SuspiciousState state);
std::vector<SuspiciousAction> suspiciousMenuActions(SuspiciousState state);
QString suspiciousActionLabel(SuspiciousAction action, SuspiciousState state,
                              const QString &name);

// GUI adapter supplies timestamps; this model never performs I/O or guesses None.
class SuspiciousUserCache
{
public:
    static constexpr std::size_t PER_ROOM_CAP = 128;
    static constexpr std::size_t TOTAL_CAP = 1024;
    static constexpr std::size_t PENDING_CAP = 128;
    SuspiciousState state(const QString &room, const QString &user) const;
    bool observe(const QString &room, const QString &user,
                 SuspiciousState state, qint64 atMs);
    std::size_t size() const;
    bool busy(const QString &room, const QString &user) const;
    std::optional<std::uint64_t> begin(const QString &room,
                                       const QString &user);
    bool complete(const QString &room, const QString &user, std::uint64_t token,
                  std::optional<SuspiciousState> success, qint64 atMs);

private:
    using Key = std::pair<QString, QString>;
    struct Entry {
        SuspiciousState state;
        qint64 atMs;
        std::uint64_t version;
    };
    struct Pending {
        std::uint64_t token;
        std::uint64_t version;
    };
    std::map<Key, Entry> entries_;
    std::map<Key, Pending> pending_;
    std::uint64_t serial_ = 0;
};
}  // namespace chatterino::satorino
