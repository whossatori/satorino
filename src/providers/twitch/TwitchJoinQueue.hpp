// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/RatelimitBucket.hpp"

#include <QHash>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>

namespace chatterino {

class TwitchJoinQueue : public QObject
{
public:
    TwitchJoinQueue(int budget, int cooldown, int confirmationTimeout,
                    std::function<void(const QString &, bool)> sendJoin);

    void request(const QString &channel);
    void confirm(const QString &channel);
    void cancel(const QString &channel);
    void reset();

private:
    void send(const QString &channel);

    const int confirmationTimeout_;
    std::function<void(const QString &, bool)> sendJoin_;
    RatelimitBucket bucket_;

    // Zero denotes a first attempt still waiting for rate limit capacity.
    QHash<QString, std::uint64_t> pending_;
    std::uint64_t nextAttempt_ = 0;
};

}  // namespace chatterino
