// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemData.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include <QJsonArray>
#    include <QJsonDocument>
#    include <QJsonObject>
#    include <QSet>

#    include <algorithm>
#    include <limits>
#    include <utility>

namespace chatterino::satorino {
namespace {

bool validChannel(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 && id != "0" &&
           std::all_of(id.begin(), id.end(), [](QChar ch) {
               return ch >= '0' && ch <= '9';
           });
}

bool validId(const QString &id)
{
    return !id.isEmpty() && id.size() <= 128;
}

QJsonObject rewardJson(const CachedRedeem &entry)
{
    const auto &r = entry.reward;
    return {{"id", r.id},
            {"title", r.title},
            {"prompt", r.prompt},
            {"type", r.rewardType},
            {"pricing", r.pricingType},
            {"color", r.backgroundColor},
            {"image", r.imageUrl},
            {"cost", r.cost},
            {"automatic", r.isAutomatic},
            {"enabled", r.isEnabled},
            {"paused", r.isPaused},
            {"stock", r.isInStock},
            {"input", r.isUserInputRequired},
            {"seen", entry.lastSeen.toString(Qt::ISODateWithMs)}};
}

GqlChannelPointReward readReward(const QJsonObject &obj)
{
    GqlChannelPointReward r;
    r.id = obj.value("id").toString();
    r.title = obj.value("title").toString().left(512);
    r.prompt = obj.value("prompt").toString().left(4096);
    r.rewardType = obj.value("type").toString().left(128);
    r.pricingType = obj.value("pricing").toString().left(32);
    r.backgroundColor = obj.value("color").toString().left(32);
    r.imageUrl = obj.value("image").toString().left(2048);
    r.cost = obj.value("cost").toInt(-1);
    r.isAutomatic = obj.value("automatic").toBool();
    r.isEnabled = obj.value("enabled").toBool();
    r.isPaused = obj.value("paused").toBool();
    r.isInStock = obj.value("stock").toBool();
    r.isUserInputRequired = obj.value("input").toBool();
    r.isCached = true;
    return r;
}

}  // namespace

QString redeemChannelKey(const QString &roomId, const QString &catalogId)
{
    const auto room = roomId.trimmed();
    const auto catalog = catalogId.trimmed();
    return validChannel(room) ? room
                              : (validChannel(catalog) ? catalog : QString{});
}

QString redeemPinId(const GqlChannelPointReward &reward)
{
    if (!reward.id.isEmpty())
    {
        return reward.id;
    }
    // Automatic rewards can omit a platform ID; this key is local only.
    return reward.isAutomatic && !reward.rewardType.isEmpty()
               ? "auto:" + reward.rewardType.trimmed().toUpper() + ":" +
                     reward.pricingType.trimmed().toUpper()
               : QString{};
}

RedeemData RedeemData::fromJson(const QString &json, const QDateTime &now)
{
    RedeemData data;
    const auto root = QJsonDocument::fromJson(json.toUtf8()).object();
    if (root.value("version").toInt() != 1)
    {
        return data;
    }
    const auto channels = root.value("channels").toObject();
    for (auto it = channels.begin(); it != channels.end(); ++it)
    {
        if (!validChannel(it.key()) || !it.value().isObject())
        {
            continue;
        }
        const auto obj = it.value().toObject();
        ChannelRedeems channel;
        channel.lastUsed = QDateTime::fromString(obj.value("used").toString(),
                                                 Qt::ISODateWithMs);
        for (const auto &value : obj.value("pins").toArray())
        {
            auto id = value.toString();
            if (validId(id) && !channel.pins.contains(id) &&
                channel.pins.size() < MAX_REWARDS)
            {
                channel.pins.push_back(id);
            }
        }
        QSet<QString> seen;
        for (const auto &value : obj.value("cache").toArray())
        {
            const auto cached = value.toObject();
            auto reward = readReward(cached);
            const auto time = QDateTime::fromString(
                cached.value("seen").toString(), Qt::ISODateWithMs);
            if (!validId(reward.id) || reward.title.isEmpty() ||
                reward.cost < 0 || seen.contains(reward.id) ||
                !time.isValid() || time > now.addSecs(300))
            {
                continue;
            }
            seen.insert(reward.id);
            channel.cache.push_back({std::move(reward), time});
        }
        data.channels_.insert(it.key(), std::move(channel));
    }
    data.trim(now);
    return data;
}

QString RedeemData::toJson() const
{
    QJsonObject channels;
    for (auto it = this->channels_.begin(); it != this->channels_.end(); ++it)
    {
        QJsonArray cache;
        for (const auto &entry : it->cache)
        {
            cache.append(rewardJson(entry));
        }
        channels.insert(
            it.key(),
            QJsonObject{
                {"pins", QJsonArray::fromStringList(it->pins)},
                {"cache", cache},
                {"used", it->lastUsed.toString(Qt::ISODateWithMs)},
                // Reserved schema only: no star UI, scheduling or auto-redemption.
                {"auto", QJsonObject{}}});
    }
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"version", 1}, {"channels", channels}})
            .toJson(QJsonDocument::Compact));
}

void RedeemData::trim(const QDateTime &now)
{
    for (auto &channel : this->channels_)
    {
        auto &cache = channel.cache;
        cache.erase(std::remove_if(cache.begin(), cache.end(),
                                   [&](const auto &entry) {
                                       return !entry.lastSeen.isValid() ||
                                              entry.lastSeen <
                                                  now.addDays(-CACHE_DAYS);
                                   }),
                    cache.end());
        std::stable_sort(cache.begin(), cache.end(),
                         [](const auto &a, const auto &b) {
                             return a.lastSeen > b.lastSeen;
                         });
        if (cache.size() > MAX_REWARDS)
        {
            cache.resize(MAX_REWARDS);
        }
    }
    // Cache eviction must never silently discard the user's pin preferences.
    int cachedChannels = 0;
    for (auto it = this->channels_.begin(); it != this->channels_.end();)
    {
        if (!it->cache.isEmpty())
        {
            ++cachedChannels;
        }
        if (it->cache.isEmpty() && it->pins.isEmpty())
        {
            it = this->channels_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    while (cachedChannels > MAX_CHANNELS)
    {
        auto oldest = this->channels_.end();
        for (auto it = this->channels_.begin(); it != this->channels_.end();
             ++it)
        {
            if (!it->cache.isEmpty() && (oldest == this->channels_.end() ||
                                         it->lastUsed < oldest->lastUsed))
            {
                oldest = it;
            }
        }
        oldest->cache.clear();
        if (oldest->pins.isEmpty())
        {
            this->channels_.erase(oldest);
        }
        --cachedChannels;
    }
}

bool RedeemData::isPinned(const QString &channelId,
                          const QString &rewardId) const
{
    const auto it = this->channels_.constFind(channelId);
    return it != this->channels_.end() && it->pins.contains(rewardId);
}

void RedeemData::setPinned(const QString &channelId, const QString &rewardId,
                           bool pinned, const QDateTime &now)
{
    if (!validChannel(channelId) || !validId(rewardId))
    {
        return;
    }
    auto &channel = this->channels_[channelId];
    channel.lastUsed = now;
    if (!pinned)
    {
        channel.pins.removeAll(rewardId);
    }
    else if (!channel.pins.contains(rewardId) &&
             channel.pins.size() < MAX_REWARDS)
    {
        channel.pins.push_back(rewardId);
    }
    this->trim(now);
}

QVector<GqlChannelPointReward> RedeemData::ordered(
    const QString &channelId, QVector<GqlChannelPointReward> rewards,
    bool enablePinning) const
{
    if (!enablePinning)
    {
        return rewards;
    }
    const auto pins = this->channels_.value(channelId).pins;
    const auto rank = [&](const QString &id) {
        const auto index = pins.indexOf(id);
        return index < 0 ? std::numeric_limits<qsizetype>::max() : index;
    };
    std::stable_sort(rewards.begin(), rewards.end(),
                     [&](const auto &a, const auto &b) {
                         return rank(redeemPinId(a)) < rank(redeemPinId(b));
                     });
    return rewards;
}

QVector<GqlChannelPointReward> RedeemData::displayOrder(
    const QString &channelId, QVector<GqlChannelPointReward> rewards,
    bool enablePinning) const
{
    std::stable_sort(
        rewards.begin(), rewards.end(), [](const auto &a, const auto &b) {
            return a.cost != b.cost
                       ? a.cost < b.cost
                       : a.title.compare(b.title, Qt::CaseInsensitive) < 0;
        });
    return this->ordered(channelId, std::move(rewards), enablePinning);
}

QVector<GqlChannelPointReward> RedeemData::cached(
    const QString &channelId) const
{
    QVector<GqlChannelPointReward> result;
    for (const auto &entry : this->channels_.value(channelId).cache)
    {
        auto reward = entry.reward;
        reward.isCached = true;
        result.push_back(std::move(reward));
    }
    return result;
}

QVector<GqlChannelPointReward> RedeemData::merge(
    const QString &channelId, const QVector<GqlChannelPointReward> &fresh,
    bool live, bool completeSnapshot, const QDateTime &now)
{
    this->trim(now);
    if (!validChannel(channelId))
    {
        return fresh;
    }
    auto &channel = this->channels_[channelId];
    channel.lastUsed = now;
    QVector<GqlChannelPointReward> result;
    QSet<QString> ids;
    QSet<QString> pinIds;
    for (auto reward : fresh)
    {
        if (!reward.id.isEmpty() && ids.contains(reward.id))
        {
            continue;
        }
        ids.insert(reward.id);
        pinIds.insert(redeemPinId(reward));
        reward.isCached = false;
        result.push_back(reward);
        if (!validId(reward.id))
        {
            continue;
        }
        auto it = std::find_if(channel.cache.begin(), channel.cache.end(),
                               [&](const auto &entry) {
                                   return entry.reward.id == reward.id;
                               });
        if (it == channel.cache.end())
        {
            channel.cache.push_back({reward, now});
        }
        else
        {
            *it = {reward, now};
        }
    }
    // Local reconciliation policy: only a successful full live catalog prunes
    // omissions. An offline/partial response is never evidence of deletion.
    if (live && completeSnapshot)
    {
        channel.cache.erase(
            std::remove_if(channel.cache.begin(), channel.cache.end(),
                           [&](const auto &entry) {
                               return !ids.contains(entry.reward.id);
                           }),
            channel.cache.end());
        channel.pins.erase(
            std::remove_if(channel.pins.begin(), channel.pins.end(),
                           [&](const auto &id) {
                               return !pinIds.contains(id);
                           }),
            channel.pins.end());
    }
    for (const auto &entry : channel.cache)
    {
        if (!ids.contains(entry.reward.id))
        {
            auto reward = entry.reward;
            reward.isCached = true;
            result.push_back(std::move(reward));
        }
    }
    this->trim(now);
    // Cache eviction never hides fresh rewards, including catalogs > the cap.
    QSet<QString> retained;
    for (const auto &entry : this->channels_.value(channelId).cache)
    {
        retained.insert(entry.reward.id);
    }
    result.erase(std::remove_if(result.begin(), result.end(),
                                [&](const auto &r) {
                                    return r.isCached &&
                                           !retained.contains(r.id);
                                }),
                 result.end());
    return result;
}

bool redeemVisible(const GqlChannelPointReward &reward, bool live,
                   const RedeemOptions &options)
{
    // The existing parser folds paused into !isEnabled, so pause takes
    // precedence over disabled; showing a paused row needs only its own toggle.
    if (reward.isPaused ? !options.showPaused
                        : (!reward.isEnabled && !options.showDisabled))
    {
        return false;
    }
    if (reward.isCached)
    {
        return !live && options.showOfflineStream;
    }
    if (!reward.isInStock)
    {
        // No distinct stream-only flag, cooldown reason or timer is retained.
        // Approximate offline unavailability separately from live cooldown/stock.
        return live ? !options.hideCooldown : options.showOfflineStream;
    }
    return true;
}

QString redeemAvailability(const GqlChannelPointReward &reward, bool live,
                           bool supported)
{
    if (reward.isCached)
    {
        return live ? "Cached - availability unverified"
                    : "Cached - unavailable while offline";
    }
    if (!supported)
    {
        return "Unsupported or disabled in settings";
    }
    if (reward.isPaused)
    {
        return "Paused";
    }
    if (!reward.isEnabled || !reward.isInStock)
    {
        if (!live)
        {
            return "Unavailable while offline";
        }
        return !reward.isEnabled ? "Disabled" : "Out of stock or on cooldown";
    }
    return {};
}

}  // namespace chatterino::satorino

#endif
