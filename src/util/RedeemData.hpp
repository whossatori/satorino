// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/api/TwitchGql.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include <QDateTime>
#    include <QMap>
#    include <QStringList>

namespace chatterino::satorino {

struct CachedRedeem {
    GqlChannelPointReward reward;
    QDateTime lastSeen;
};

struct ChannelRedeems {
    QStringList pins;
    QVector<CachedRedeem> cache;
    QDateTime lastUsed;
};

// Local presentation/preferences only. No network or redemption operations.
class RedeemData
{
public:
    static constexpr int MAX_CHANNELS = 32;
    static constexpr int MAX_REWARDS = 128;
    static constexpr int CACHE_DAYS = 30;

    static RedeemData fromJson(const QString &json, const QDateTime &now);
    QString toJson() const;
    bool isPinned(const QString &channelId, const QString &rewardId) const;
    void setPinned(const QString &channelId, const QString &rewardId,
                   bool pinned, const QDateTime &now);
    QVector<GqlChannelPointReward> ordered(
        const QString &channelId, QVector<GqlChannelPointReward> rewards) const;
    QVector<GqlChannelPointReward> cached(const QString &channelId) const;
    QVector<GqlChannelPointReward> merge(
        const QString &channelId, const QVector<GqlChannelPointReward> &fresh,
        bool live, bool completeSnapshot, const QDateTime &now);

private:
    void trim(const QDateTime &now);
    QMap<QString, ChannelRedeems> channels_;
};

QString redeemAvailability(const GqlChannelPointReward &reward, bool live,
                           bool supported = true);

}  // namespace chatterino::satorino

#endif
