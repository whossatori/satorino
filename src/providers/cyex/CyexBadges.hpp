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
#include <vector>

class QDateTime;

namespace chatterino {

class NetworkResult;

namespace cyex {

struct BadgeKind {
    QString title;
    QString description;
    EmotePtr badge;
};

using Catalog = std::unordered_map<QString, BadgeKind>;
using UserBadges = std::unordered_map<QString, std::vector<EmotePtr>>;

std::optional<Catalog> parseCatalog(const QByteArray &body);
std::optional<UserBadges> parseUsers(const QByteArray &body,
                                     const Catalog &catalog);
qint64 retryAfterDelayMs(const QByteArray &header, const QDateTime &now);

}  // namespace cyex

class CyexBadges final : public QObject
{
public:
    void initialize();
    std::vector<EmotePtr> getBadges(const QString &twitchId) const;

private:
    void refresh();
    void handleResponse(const NetworkResult &result);
    void schedule(qint64 delayMs);

    QTimer refreshTimer_;
    pajlada::Signals::SignalHolder connections_;
    qint64 remainingDelayMs_ = 0;
    bool requestInFlight_ = false;
    std::optional<cyex::Catalog> catalog_;
    QByteArray etag_;
    mutable std::shared_mutex mutex_;
    cyex::UserBadges users_;
};

}  // namespace chatterino
