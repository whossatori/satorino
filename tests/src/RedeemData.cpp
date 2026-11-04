// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemData.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "Test.hpp"

#    include <QJsonArray>
#    include <QJsonDocument>
#    include <QJsonObject>

using namespace chatterino;
using namespace chatterino::satorino;

namespace {

const auto NOW = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);

GqlChannelPointReward reward(const QString &id)
{
    GqlChannelPointReward r;
    r.id = id;
    r.title = "Reward " + id;
    r.prompt = "Message";
    r.rewardType = "CUSTOM_REWARD";
    r.pricingType = "POINTS";
    r.cost = 100;
    r.isEnabled = true;
    r.isInStock = true;
    return r;
}

QStringList ids(const QVector<GqlChannelPointReward> &rewards)
{
    QStringList result;
    for (const auto &r : rewards)
    {
        result.push_back(r.id);
    }
    return result;
}

QJsonObject json(const RedeemData &data)
{
    return QJsonDocument::fromJson(data.toJson().toUtf8()).object();
}

}  // namespace

TEST(RedeemData, PinnedFirstInPinOrderAndUnpinnedOrderIsStable)
{
    RedeemData data;
    data.setPinned("123", "c", true, NOW);
    data.setPinned("123", "a", true, NOW);
    auto result = data.ordered(
        "123", {reward("a"), reward("b"), reward("c"), reward("d")});
    EXPECT_EQ(ids(result), (QStringList{"c", "a", "b", "d"}));
}

TEST(RedeemData, RepeatedPinDoesNotChangeOrder)
{
    RedeemData data;
    data.setPinned("123", "b", true, NOW);
    data.setPinned("123", "a", true, NOW);
    data.setPinned("123", "b", true, NOW);
    EXPECT_EQ(ids(data.ordered("123", {reward("a"), reward("b")})),
              (QStringList{"b", "a"}));
}

TEST(RedeemData, UnpinAndRepinAppendsToPinOrder)
{
    RedeemData data;
    data.setPinned("123", "a", true, NOW);
    data.setPinned("123", "b", true, NOW);
    data.setPinned("123", "a", false, NOW);
    EXPECT_FALSE(data.isPinned("123", "a"));
    EXPECT_EQ(ids(data.ordered("123", {reward("a"), reward("c"), reward("b")})),
              (QStringList{"b", "a", "c"}));
    data.setPinned("123", "a", true, NOW);
    EXPECT_EQ(ids(data.ordered("123", {reward("a"), reward("b")})),
              (QStringList{"b", "a"}));
}

TEST(RedeemData, ChannelPinsAreIndependent)
{
    RedeemData data;
    data.setPinned("123", "b", true, NOW);
    data.setPinned("456", "a", true, NOW);
    const QVector<GqlChannelPointReward> list{reward("a"), reward("b")};
    EXPECT_EQ(ids(data.ordered("123", list)), (QStringList{"b", "a"}));
    EXPECT_EQ(ids(data.ordered("456", list)), (QStringList{"a", "b"}));
    EXPECT_EQ(ids(data.ordered("789", list)), ids(list));
}

TEST(RedeemData, MissingPinnedRewardNeverCreatesAGhostRow)
{
    RedeemData data;
    data.setPinned("123", "gone", true, NOW);
    EXPECT_TRUE(data.cached("123").isEmpty());
    EXPECT_EQ(ids(data.ordered("123", {reward("a")})), (QStringList{"a"}));
}

TEST(RedeemData, PersistenceRoundTripPreservesPinsAndCachedDetails)
{
    RedeemData data;
    auto r = reward("a");
    r.imageUrl = "https://example.invalid/reward.png";
    r.isUserInputRequired = true;
    r.isPaused = true;
    data.merge("123", {r}, true, true, NOW);
    data.setPinned("123", r.id, true, NOW);
    const auto restored = RedeemData::fromJson(data.toJson(), NOW.addSecs(1));
    ASSERT_TRUE(restored.isPinned("123", "a"));
    const auto cached = restored.cached("123");
    ASSERT_EQ(cached.size(), 1);
    EXPECT_EQ(cached[0].title, r.title);
    EXPECT_EQ(cached[0].cost, r.cost);
    EXPECT_EQ(cached[0].imageUrl, r.imageUrl);
    EXPECT_EQ(cached[0].prompt, r.prompt);
    EXPECT_TRUE(cached[0].isUserInputRequired);
    EXPECT_TRUE(cached[0].isPaused);
    EXPECT_TRUE(cached[0].isCached);
    EXPECT_FALSE(redeemAvailability(cached[0], false).isEmpty());
}

TEST(RedeemData, MissingAndGarbagePersistenceIsSafe)
{
    for (const auto &input :
         {"", "garbage", "null", "[]", "{}", "{\"version\":2}",
          "{\"version\":1,\"channels\":42}"})
    {
        const auto data = RedeemData::fromJson(input, NOW);
        EXPECT_FALSE(data.isPinned("123", "a"));
        EXPECT_TRUE(data.cached("123").isEmpty());
    }
}

TEST(RedeemData, GarbageEntriesAndDuplicatePinsAreIgnored)
{
    const auto root = QJsonObject{
        {"version", 1},
        {"channels",
         QJsonObject{
             {"123",
              QJsonObject{
                  {"pins",
                   QJsonArray{1, QJsonValue{}, "a", "a", QJsonObject{}}},
                  {"cache", QJsonArray{1, QJsonObject{{"id", "broken"}}}}}},
             {"display-name", QJsonObject{{"pins", QJsonArray{"a"}}}}}}};
    const auto data = RedeemData::fromJson(
        QString::fromUtf8(QJsonDocument(root).toJson()), NOW);
    EXPECT_TRUE(data.isPinned("123", "a"));
    EXPECT_TRUE(data.cached("123").isEmpty());
    EXPECT_FALSE(data.isPinned("display-name", "a"));
    EXPECT_EQ(json(data)
                  .value("channels")
                  .toObject()
                  .value("123")
                  .toObject()
                  .value("pins")
                  .toArray()
                  .size(),
              1);
}

TEST(RedeemData, OfflineOmissionRetainsNonActionableCachedRewardAndPin)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    data.setPinned("123", "a", true, NOW);
    const auto result = data.merge("123", {}, false, true, NOW.addSecs(10));
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(result[0].isCached);
    EXPECT_EQ(redeemAvailability(result[0], false),
              "Cached - unavailable while offline");
    EXPECT_TRUE(data.isPinned("123", "a"));
}

TEST(RedeemData, PartialLiveResponseDoesNotProveDeletion)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    const auto result = data.merge("123", {}, true, false, NOW.addSecs(1));
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(result[0].isCached);
    EXPECT_EQ(redeemAvailability(result[0], true),
              "Cached - availability unverified");
}

TEST(RedeemData, CompleteLiveOmissionPrunesDeletedRewardAndPin)
{
    RedeemData data;
    data.merge("123", {reward("a"), reward("b")}, true, true, NOW);
    data.setPinned("123", "a", true, NOW);
    const auto result =
        data.merge("123", {reward("b")}, true, true, NOW.addSecs(1));
    EXPECT_EQ(ids(result), (QStringList{"b"}));
    EXPECT_EQ(ids(data.cached("123")), (QStringList{"b"}));
    EXPECT_FALSE(data.isPinned("123", "a"));
}

TEST(RedeemData, CompleteEmptyLiveCatalogPrunesAllRows)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    data.setPinned("123", "a", true, NOW);
    EXPECT_TRUE(data.merge("123", {}, true, true, NOW.addSecs(1)).isEmpty());
    EXPECT_TRUE(data.cached("123").isEmpty());
    EXPECT_FALSE(data.isPinned("123", "a"));
}

TEST(RedeemData, FreshDataReplacesCachedFlagsTitleAndCostWithoutDuplicates)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    auto newer = reward("a");
    newer.title = "New title";
    newer.cost = 200;
    newer.isInStock = false;
    const auto result =
        data.merge("123", {newer, newer}, false, true, NOW.addSecs(1));
    ASSERT_EQ(result.size(), 1);
    EXPECT_FALSE(result[0].isCached);
    EXPECT_EQ(result[0].title, newer.title);
    EXPECT_EQ(result[0].cost, 200);
    EXPECT_FALSE(result[0].isInStock);
    EXPECT_EQ(redeemAvailability(result[0], false),
              "Unavailable while offline");
}

TEST(RedeemData, CacheExpiresAfterThirtyDaysWithoutGhostRows)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    data.setPinned("123", "a", true, NOW);
    const auto restored = RedeemData::fromJson(
        data.toJson(), NOW.addDays(RedeemData::CACHE_DAYS + 1));
    EXPECT_TRUE(restored.cached("123").isEmpty());
    EXPECT_TRUE(restored.ordered("123", {}).isEmpty());
}

TEST(RedeemData, CacheCapDoesNotHideFreshRewards)
{
    RedeemData data;
    QVector<GqlChannelPointReward> fresh;
    for (int i = 0; i < RedeemData::MAX_REWARDS + 10; ++i)
    {
        fresh.push_back(reward(QString::number(i)));
    }
    const auto result = data.merge("123", fresh, true, true, NOW);
    EXPECT_EQ(result.size(), fresh.size());
    EXPECT_EQ(data.cached("123").size(), RedeemData::MAX_REWARDS);
    const auto offline = data.merge("123", {}, false, true, NOW.addSecs(1));
    EXPECT_EQ(offline.size(), RedeemData::MAX_REWARDS);
}

TEST(RedeemData, ChannelAndPinCountsAreBounded)
{
    RedeemData data;
    for (int i = 1; i <= RedeemData::MAX_CHANNELS + 2; ++i)
    {
        data.merge(QString::number(i), {reward("a")}, true, true,
                   NOW.addSecs(i));
    }
    EXPECT_EQ(json(data).value("channels").toObject().size(),
              RedeemData::MAX_CHANNELS);
    EXPECT_TRUE(data.cached("1").isEmpty());
    for (int i = 0; i < RedeemData::MAX_REWARDS + 2; ++i)
    {
        data.setPinned("123", QString::number(i), true, NOW.addSecs(100));
    }
    EXPECT_EQ(json(data)
                  .value("channels")
                  .toObject()
                  .value("123")
                  .toObject()
                  .value("pins")
                  .toArray()
                  .size(),
              RedeemData::MAX_REWARDS);
}

TEST(RedeemData, PausedStockAndUnsupportedStatusesAreExplicit)
{
    auto r = reward("a");
    EXPECT_TRUE(redeemAvailability(r, true).isEmpty());
    r.isPaused = true;
    EXPECT_EQ(redeemAvailability(r, true), "Paused");
    r.isPaused = false;
    r.isInStock = false;
    EXPECT_EQ(redeemAvailability(r, true), "Out of stock or on cooldown");
    EXPECT_EQ(redeemAvailability(r, false), "Unavailable while offline");
    r.isEnabled = false;
    EXPECT_EQ(redeemAvailability(r, true), "Disabled");
    EXPECT_FALSE(redeemAvailability(reward("a"), true, false).isEmpty());
}

TEST(RedeemData, AutoKeyIsReservedAndHasNoEffect)
{
    RedeemData data;
    data.merge("123", {reward("a")}, true, true, NOW);
    auto root = json(data);
    auto channels = root.value("channels").toObject();
    auto channel = channels.value("123").toObject();
    EXPECT_TRUE(channel.value("auto").toObject().isEmpty());
    channel.insert("auto", QJsonObject{{"a", true}});
    channels.insert("123", channel);
    root.insert("channels", channels);
    auto restored = RedeemData::fromJson(
        QString::fromUtf8(QJsonDocument(root).toJson()), NOW);
    EXPECT_FALSE(restored.isPinned("123", "a"));
    const auto cached = restored.cached("123");
    ASSERT_EQ(cached.size(), 1);
    EXPECT_TRUE(cached[0].isCached);
    EXPECT_TRUE(json(restored)
                    .value("channels")
                    .toObject()
                    .value("123")
                    .toObject()
                    .value("auto")
                    .toObject()
                    .isEmpty());
}

TEST(RedeemData, CacheEvictionPreservesPinPreferences)
{
    RedeemData data;
    data.merge("1", {reward("a")}, true, true, NOW);
    data.setPinned("1", "a", true, NOW);
    for (int i = 2; i <= RedeemData::MAX_CHANNELS + 1; ++i)
    {
        data.merge(QString::number(i), {reward("b")}, true, true,
                   NOW.addSecs(i));
    }
    EXPECT_TRUE(data.cached("1").isEmpty());
    EXPECT_TRUE(data.isPinned("1", "a"));
    const auto restored = RedeemData::fromJson(data.toJson(), NOW.addSecs(100));
    EXPECT_TRUE(restored.isPinned("1", "a"));
    EXPECT_TRUE(restored.cached("1").isEmpty());
}

#endif
