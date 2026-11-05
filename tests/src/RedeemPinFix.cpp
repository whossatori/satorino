// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemData.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "Test.hpp"

using namespace chatterino;
using namespace chatterino::satorino;

namespace {
GqlChannelPointReward reward(const QString &id, int cost)
{
    GqlChannelPointReward r;
    r.id = id;
    r.title = id;
    r.cost = cost;
    return r;
}
QStringList ids(const QVector<GqlChannelPointReward> &rewards)
{
    QStringList result;
    for (const auto &r : rewards)
        result.push_back(redeemPinId(r));
    return result;
}
}  // namespace

TEST(RedeemPinFix, FinalDisplaySortKeepsPinOrderAheadOfCostAndTitle)
{
    RedeemData data;
    const auto now = QDateTime::currentDateTimeUtc();
    data.setPinned("123", "expensive", true, now);
    data.setPinned("123", "second", true, now);
    const QVector<GqlChannelPointReward> input{
        reward("cheap", 1), reward("second", 100), reward("expensive", 1000),
        reward("other", 2)};
    EXPECT_EQ(ids(data.displayOrder("123", input)),
              (QStringList{"expensive", "second", "cheap", "other"}));
    EXPECT_EQ(ids(data.displayOrder("123", input, false)),
              (QStringList{"cheap", "other", "second", "expensive"}));
    EXPECT_TRUE(data.isPinned("123", "expensive"));
}

TEST(RedeemPinFix, ToggleTwiceRoundTripsAndMergePreservesPins)
{
    const auto now = QDateTime::currentDateTimeUtc();
    RedeemData data;
    const auto r = reward("id", 100);
    data.merge("123", {r}, true, true, now);
    data.setPinned("123", "id", true, now);
    data.merge("123", {r}, true, true, now.addSecs(1));
    auto restored = RedeemData::fromJson(data.toJson(), now.addSecs(2));
    EXPECT_TRUE(restored.isPinned("123", "id"));
    restored.setPinned("123", "id", false, now.addSecs(3));
    restored = RedeemData::fromJson(restored.toJson(), now.addSecs(4));
    EXPECT_FALSE(restored.isPinned("123", "id"));
}

TEST(RedeemPinFix, ChannelKeyUsesValidRoomIdThenValidCatalogId)
{
    EXPECT_EQ(redeemChannelKey(" 123 ", "456"), "123");
    EXPECT_EQ(redeemChannelKey("", "456"), "456");
    EXPECT_EQ(redeemChannelKey("invalid", "456"), "456");
    EXPECT_EQ(redeemChannelKey("0", "456"), "456");
    EXPECT_TRUE(redeemChannelKey("invalid", "display name").isEmpty());
}

TEST(RedeemPinFix, AutomaticRewardWithoutPlatformIdHasStableLocalPinKey)
{
    auto r = reward({}, 100);
    r.isAutomatic = true;
    r.rewardType = "SEND_HIGHLIGHTED_MESSAGE";
    r.pricingType = "POINTS";
    const auto key = redeemPinId(r);
    ASSERT_FALSE(key.isEmpty());
    RedeemData data;
    const auto now = QDateTime::currentDateTimeUtc();
    data.setPinned("123", key, true, now);
    data.merge("123", {r}, true, true, now);
    EXPECT_TRUE(data.isPinned("123", key));
    EXPECT_EQ(redeemPinId(r), key);
    r.pricingType = "points";
    EXPECT_EQ(redeemPinId(r), key);
    r.isAutomatic = false;
    EXPECT_TRUE(redeemPinId(r).isEmpty());
}

TEST(RedeemPinFix, UnpinAndRepinAppendWhileMergeAndRoundTripKeepOrder)
{
    const auto now = QDateTime::currentDateTimeUtc();
    RedeemData data;
    const QVector<GqlChannelPointReward> rewards{
        reward("a", 10), reward("b", 20), reward("c", 1)};
    data.setPinned("123", "a", true, now);
    data.setPinned("123", "b", true, now);
    data.setPinned("123", "a", false, now);
    data.setPinned("123", "a", true, now);
    data.merge("123", rewards, true, true, now);
    const auto restored = RedeemData::fromJson(data.toJson(), now);
    EXPECT_EQ(ids(restored.displayOrder("123", rewards)),
              (QStringList{"b", "a", "c"}));
    EXPECT_EQ(ids(restored.displayOrder("123", rewards, false)),
              (QStringList{"c", "a", "b"}));
    EXPECT_TRUE(restored.isPinned("123", "a"));
    EXPECT_EQ(ids(restored.displayOrder("123", rewards, true)),
              (QStringList{"b", "a", "c"}));
}
TEST(RedeemPinFix, EmptyChannelOrRewardNeverCreatesAPin)
{
    RedeemData data;
    const auto now = QDateTime::currentDateTimeUtc();
    data.setPinned("", "id", true, now);
    data.setPinned("123", "", true, now);
    EXPECT_FALSE(data.isPinned("", "id"));
    EXPECT_FALSE(data.isPinned("123", ""));
}

#endif
