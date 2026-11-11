// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/Emote.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <pajlada/signals/signalholder.hpp>

#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace chatterino {

class NetworkResult;

namespace satorino {

using BadgeAssignments = std::unordered_map<QString, std::vector<EmotePtr>>;

std::optional<BadgeAssignments> parseBadgeFeed(const QByteArray &body);

}  // namespace satorino

class SatorinoBadges final : public QObject
{
public:
    void initialize();
    void refresh();
    std::vector<EmotePtr> getBadges(const QString &twitchId) const;

private:
    void requestFeed(bool legacy);
    void handleResponse(const NetworkResult &result, bool legacy);

    pajlada::Signals::SignalHolder connections_;
    bool requestInFlight_ = false;
    QByteArray etag_;
    mutable std::shared_mutex mutex_;
    satorino::BadgeAssignments users_;
};

}  // namespace chatterino
