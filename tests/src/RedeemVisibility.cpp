// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemData.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "Test.hpp"

using namespace chatterino;
using namespace chatterino::satorino;

namespace {

GqlChannelPointReward availableReward(const QString &id = "reward")
{
    GqlChannelPointReward r;
    r.id = id;
    r.title = id;
    r.isEnabled = true;
    r.isInStock = true;
    r.cost = 100;
    return r;
}

QStringList ids(const QVector<GqlChannelPointReward> &rewards)
{
    QStringList result;
    for (const auto &reward : rewards)
    {
        result.push_back(reward.id);
    }
    return result;
}

}  // namespace

TEST(RedeemVisibility, DefaultsMatchRequestedCheckboxValues)
{
    const RedeemOptions options;
    EXPECT_FALSE(options.showDisabled);
    EXPECT_FALSE(options.showPaused);
    EXPECT_FALSE(options.showOfflineStream);
    EXPECT_FALSE(options.hideCooldown);
    EXPECT_TRUE(options.enablePinning);
}

TEST(RedeemVisibility, DefaultFieldMatrix)
{
    struct Row {
        bool enabled;
        bool paused;
        bool stock;
        bool cached;
        bool offlineVisible;
        bool liveVisible;
    };
    // Every enabled/paused/stock/cache combination, in both channel states.
    const Row cases[] = {
        {false, false, false, false, false, false},
        {false, false, true, false, false, false},
        {false, true, false, false, false, false},
        {false, true, true, false, false, false},
        {true, false, false, false, false, true},
        {true, false, true, false, true, true},
        {true, true, false, false, false, false},
        {true, true, true, false, false, false},
        {false, false, false, true, false, false},
        {false, false, true, true, false, false},
        {false, true, false, true, false, false},
        {false, true, true, true, false, false},
        {true, false, false, true, false, false},
        {true, false, true, true, false, false},
        {true, true, false, true, false, false},
        {true, true, true, true, false, false},
    };
    for (const auto &row : cases)
    {
        auto r = availableReward();
        r.isEnabled = row.enabled;
        r.isPaused = row.paused;
        r.isInStock = row.stock;
        r.isCached = row.cached;
        EXPECT_EQ(redeemVisible(r, false), row.offlineVisible);
        EXPECT_EQ(redeemVisible(r, true), row.liveVisible);
    }
}

TEST(RedeemVisibility, ShowDisabledOnlyRevealsDisabledRows)
{
    auto disabled = availableReward();
    disabled.isEnabled = false;
    auto paused = availableReward();
    paused.isPaused = true;
    RedeemOptions options;
    options.showDisabled = true;
    EXPECT_TRUE(redeemVisible(disabled, true, options));
    EXPECT_FALSE(redeemVisible(paused, true, options));
    EXPECT_EQ(redeemAvailability(disabled, true), "Disabled");
}

TEST(RedeemVisibility, ShowPausedHandlesExistingFoldedEnabledField)
{
    RedeemOptions options;
    options.showPaused = true;
    for (bool enabled : {false, true})
    {
        auto paused = availableReward();
        paused.isEnabled = enabled;
        paused.isPaused = true;
        EXPECT_TRUE(redeemVisible(paused, true, options));
        EXPECT_EQ(redeemAvailability(paused, true), "Paused");
    }
    auto disabled = availableReward();
    disabled.isEnabled = false;
    EXPECT_FALSE(redeemVisible(disabled, true, options));
}

TEST(RedeemVisibility, OfflineStreamToggleRevealsUnavailableAndCachedRows)
{
    auto offline = availableReward();
    offline.isInStock = false;
    auto cached = availableReward();
    cached.isCached = true;
    RedeemOptions options;
    EXPECT_FALSE(redeemVisible(offline, false, options));
    EXPECT_FALSE(redeemVisible(cached, false, options));
    options.showOfflineStream = true;
    EXPECT_TRUE(redeemVisible(offline, false, options));
    EXPECT_TRUE(redeemVisible(cached, false, options));
    EXPECT_FALSE(redeemAvailability(offline, false).isEmpty());
    EXPECT_FALSE(redeemAvailability(cached, false).isEmpty());
    // A cached omission while live is unknown, not proof of a live-only reward.
    EXPECT_FALSE(redeemVisible(cached, true, options));
}

TEST(RedeemVisibility, OfflineToggleDoesNotOverrideDisabledOrPausedFilters)
{
    RedeemOptions options;
    options.showOfflineStream = true;
    auto r = availableReward();
    r.isCached = true;
    r.isEnabled = false;
    EXPECT_FALSE(redeemVisible(r, false, options));
    options.showDisabled = true;
    EXPECT_TRUE(redeemVisible(r, false, options));
    r.isPaused = true;
    EXPECT_FALSE(redeemVisible(r, false, options));
    options.showPaused = true;
    EXPECT_TRUE(redeemVisible(r, false, options));
}

TEST(RedeemVisibility, CooldownIsShownGreyByDefaultAndCanBeHidden)
{
    auto r = availableReward();
    r.isInStock = false;
    RedeemOptions options;
    EXPECT_TRUE(redeemVisible(r, true, options));
    EXPECT_EQ(redeemAvailability(r, true), "Out of stock or on cooldown");
    options.hideCooldown = true;
    EXPECT_FALSE(redeemVisible(r, true, options));
    EXPECT_TRUE(redeemVisible(availableReward(), true, options));
}

TEST(RedeemVisibility, HideCooldownDoesNotHideSeparateOfflineCandidates)
{
    auto r = availableReward();
    r.isInStock = false;
    RedeemOptions options;
    options.showOfflineStream = true;
    options.hideCooldown = true;
    EXPECT_TRUE(redeemVisible(r, false, options));
    EXPECT_FALSE(redeemVisible(r, true, options));
}

TEST(RedeemVisibility, UserInputRequirementDoesNotChangeVisibility)
{
    auto r = availableReward();
    for (bool inputRequired : {false, true})
    {
        r.isUserInputRequired = inputRequired;
        EXPECT_TRUE(redeemVisible(r, true));
        EXPECT_TRUE(redeemVisible(r, false));
    }
}

TEST(RedeemVisibility, PinningOffIgnoresOrderWithoutDeletingPreferences)
{
    const auto now = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);
    RedeemData data;
    data.setPinned("123", "c", true, now);
    data.setPinned("123", "a", true, now);
    const auto before = data.toJson();
    const QVector<GqlChannelPointReward> rewards{
        availableReward("a"), availableReward("b"), availableReward("c"),
        availableReward("d")};
    EXPECT_EQ(ids(data.ordered("123", rewards, false)), ids(rewards));
    EXPECT_EQ(data.toJson(), before);
    EXPECT_TRUE(data.isPinned("123", "c"));
    EXPECT_EQ(ids(data.ordered("123", rewards, true)),
              (QStringList{"c", "a", "b", "d"}));
    const auto restored = RedeemData::fromJson(data.toJson(), now);
    EXPECT_EQ(ids(restored.ordered("123", rewards, true)),
              (QStringList{"c", "a", "b", "d"}));
}

TEST(RedeemVisibility, PinsDoNotMakeFilteredRewardsVisible)
{
    const auto now = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);
    RedeemData data;
    auto r = availableReward();
    r.isEnabled = false;
    data.setPinned("123", r.id, true, now);
    EXPECT_TRUE(data.isPinned("123", r.id));
    EXPECT_FALSE(redeemVisible(r, true));
    EXPECT_TRUE(data.ordered("123", {}).isEmpty());
}

TEST(RedeemVisibility, RevealingCachedRowsNeverMakesThemRedeemable)
{
    const auto now = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);
    RedeemData data;
    data.merge("123", {availableReward()}, true, true, now);
    const auto restored = RedeemData::fromJson(data.toJson(), now.addSecs(1));
    const auto cached = restored.cached("123");
    ASSERT_EQ(cached.size(), 1);
    RedeemOptions options;
    options.showOfflineStream = true;
    EXPECT_TRUE(redeemVisible(cached[0], false, options));
    EXPECT_FALSE(redeemAvailability(cached[0], false).isEmpty());
}

#endif
