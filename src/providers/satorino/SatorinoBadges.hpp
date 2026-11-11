// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/Emote.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <pajlada/signals/signalholder.hpp>

#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace chatterino {

class NetworkResult;

namespace satorino {

using BadgeAssignments = std::unordered_map<QString, EmotePtr>;

std::optional<BadgeAssignments> parseBadgeFeed(const QByteArray &body);

}  // namespace satorino

class SatorinoBadges final : public QObject
{
public:
    void initialize();
    EmotePtr getBadge(const QString &twitchId) const;

private:
    void refresh();
    void handleResponse(const NetworkResult &result);

    QTimer refreshTimer_;
    pajlada::Signals::SignalHolder connections_;
    bool requestInFlight_ = false;
    QByteArray etag_;
    mutable std::shared_mutex mutex_;
    satorino::BadgeAssignments users_;
};

}  // namespace chatterino
