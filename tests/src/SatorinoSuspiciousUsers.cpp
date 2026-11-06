// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoSuspiciousUsers.hpp"
#include "Test.hpp"
#include <QString>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

using namespace chatterino::satorino;

TEST(SatorinoSuspiciousUsers,
     VisibilityRequiresRealTwitchModAndKnownRegularTarget)
{
    for (const auto role :
         {FlagTargetRole::Unknown, FlagTargetRole::Self,
          FlagTargetRole::Broadcaster, FlagTargetRole::Moderator})
        EXPECT_FALSE(flagControlVisible(true, true, role));
    EXPECT_TRUE(flagControlVisible(
        true, true,
        FlagTargetRole::Regular));  // Moderator or broadcaster account.
    EXPECT_FALSE(
        flagControlVisible(true, false, FlagTargetRole::Regular));  // Viewer.
    EXPECT_FALSE(
        flagControlVisible(false, true, FlagTargetRole::Regular));  // Kick.
    EXPECT_FALSE(flagControlVisible(
        false, false, FlagTargetRole::Regular));  // YouTube/viewer.
}
TEST(SatorinoSuspiciousUsers,
     MenusDistinguishKnownNoneMonitoredRestrictedAndUnknown)
{
    using A = SuspiciousAction;
    EXPECT_EQ(suspiciousMenuActions(SuspiciousState::None),
              (std::vector<A>{A::Monitor, A::Restrict}));
    EXPECT_EQ(suspiciousMenuActions(SuspiciousState::Monitored),
              (std::vector<A>{A::Restrict, A::Remove}));
    EXPECT_EQ(suspiciousMenuActions(SuspiciousState::Restricted),
              (std::vector<A>{A::Remove}));
    EXPECT_EQ(suspiciousMenuActions(SuspiciousState::Unknown),
              (std::vector<A>{A::Monitor, A::Restrict, A::Remove}));
    EXPECT_EQ(
        suspiciousActionLabel(A::Monitor, SuspiciousState::Unknown, "target"),
        "Monitor target");
    EXPECT_EQ(
        suspiciousActionLabel(A::Restrict, SuspiciousState::None, "target"),
        "Restrict target");
    EXPECT_EQ(
        suspiciousActionLabel(A::Remove, SuspiciousState::Monitored, "target"),
        "Stop monitoring");
    EXPECT_EQ(
        suspiciousActionLabel(A::Remove, SuspiciousState::Restricted, "target"),
        "Stop restricting");
    EXPECT_EQ(
        suspiciousActionLabel(A::Remove, SuspiciousState::Unknown, "target"),
        "Remove flag");
    EXPECT_EQ(suspiciousControlLabel(SuspiciousState::Unknown), "Flag user");
    EXPECT_EQ(suspiciousControlLabel(SuspiciousState::None), "Flag user");
    EXPECT_EQ(suspiciousControlLabel(SuspiciousState::Monitored), "Monitored");
    EXPECT_EQ(suspiciousControlLabel(SuspiciousState::Restricted),
              "Restricted");
}
TEST(SatorinoSuspiciousUsers,
     MissingStaleAndInvalidObservationsNeverGuessUnflagged)
{
    SuspiciousUserCache cache;
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::Unknown);
    EXPECT_FALSE(cache.observe("bad", "200", SuspiciousState::None, 100));
    EXPECT_FALSE(cache.observe("100", "bad", SuspiciousState::None, 100));
    EXPECT_FALSE(cache.observe("100", "200", SuspiciousState::Unknown, 100));
    EXPECT_FALSE(cache.observe("100", "200", SuspiciousState::Monitored, 0));
    EXPECT_EQ(cache.size(), 0);
    ASSERT_TRUE(cache.observe("100", "200", SuspiciousState::Monitored, 100));
    EXPECT_FALSE(cache.observe("100", "200", SuspiciousState::Restricted, 99));
    EXPECT_FALSE(cache.observe("100", "200", SuspiciousState::Monitored, 100));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::Monitored);
    EXPECT_EQ(cache.state("101", "200"), SuspiciousState::Unknown);
    ASSERT_TRUE(cache.observe("100", "200", SuspiciousState::None, 101));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::None);
}
TEST(SatorinoSuspiciousUsers,
     PerRoomCapEvictsOldestObservationWithoutReadPromotion)
{
    SuspiciousUserCache cache;
    for (std::size_t i = 1; i <= SuspiciousUserCache::PER_ROOM_CAP; ++i)
        ASSERT_TRUE(cache.observe("100",
                                  QString::number(static_cast<qulonglong>(i)),
                                  SuspiciousState::Monitored, 100));
    EXPECT_EQ(cache.state("100", "1"), SuspiciousState::Monitored);
    ASSERT_TRUE(cache.observe("101", "1", SuspiciousState::Restricted, 100));
    ASSERT_TRUE(cache.observe("100", "9999", SuspiciousState::None, 101));
    EXPECT_EQ(cache.size(), SuspiciousUserCache::PER_ROOM_CAP + 1);
    EXPECT_EQ(cache.state("100", "1"), SuspiciousState::Unknown);
    EXPECT_EQ(cache.state("101", "1"), SuspiciousState::Restricted);
    ASSERT_TRUE(cache.observe("100", "2", SuspiciousState::Restricted, 102));
    ASSERT_TRUE(cache.observe("100", "9998", SuspiciousState::Monitored, 103));
    EXPECT_EQ(cache.state("100", "2"), SuspiciousState::Restricted);
    EXPECT_EQ(cache.state("100", "3"), SuspiciousState::Unknown);
}
TEST(SatorinoSuspiciousUsers, TotalCapBoundsManyRooms)
{
    SuspiciousUserCache cache;
    for (std::size_t i = 1; i <= SuspiciousUserCache::TOTAL_CAP + 1; ++i)
        ASSERT_TRUE(cache.observe(QString::number(static_cast<qulonglong>(i)),
                                  "200", SuspiciousState::Monitored, 100));
    EXPECT_EQ(cache.size(), SuspiciousUserCache::TOTAL_CAP);
    EXPECT_EQ(cache.state("1", "200"), SuspiciousState::Unknown);
    EXPECT_EQ(cache.state("2", "200"), SuspiciousState::Monitored);
}
TEST(SatorinoSuspiciousUsers,
     RequestGateRejectsDuplicatesAndFailureDoesNotChangeState)
{
    SuspiciousUserCache cache;
    ASSERT_TRUE(cache.observe("100", "200", SuspiciousState::None, 100));
    const auto token = cache.begin("100", "200");
    ASSERT_TRUE(token);
    EXPECT_TRUE(cache.busy("100", "200"));
    EXPECT_FALSE(cache.begin("100", "200"));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::None);
    EXPECT_FALSE(cache.complete("100", "200", *token + 1,
                                SuspiciousState::Restricted, 101));
    EXPECT_TRUE(cache.busy("100", "200"));
    EXPECT_TRUE(cache.complete("100", "200", *token, std::nullopt, 102));
    EXPECT_FALSE(cache.busy("100", "200"));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::None);
    EXPECT_TRUE(cache.begin("100", "200"));
}
TEST(SatorinoSuspiciousUsers,
     SuccessUpdatesStateButLaterFeedWinsAndOldCompletionCannotUnlockNewRequest)
{
    SuspiciousUserCache cache;
    auto token = cache.begin("100", "200");
    ASSERT_TRUE(token);
    ASSERT_TRUE(
        cache.complete("100", "200", *token, SuspiciousState::Monitored, 100));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::Monitored);
    const auto old = *token;
    token = cache.begin("100", "200");
    ASSERT_TRUE(token);
    EXPECT_FALSE(
        cache.complete("100", "200", old, SuspiciousState::Restricted, 101));
    EXPECT_TRUE(cache.busy("100", "200"));
    ASSERT_TRUE(cache.observe("100", "200", SuspiciousState::None, 102));
    ASSERT_TRUE(
        cache.complete("100", "200", *token, SuspiciousState::Restricted, 103));
    EXPECT_EQ(cache.state("100", "200"), SuspiciousState::None);
    EXPECT_FALSE(cache.busy("100", "200"));
    SuspiciousUserCache skewed;
    ASSERT_TRUE(skewed.observe("100", "200", SuspiciousState::None, 1000));
    const auto skewedToken = skewed.begin("100", "200");
    ASSERT_TRUE(skewedToken);
    ASSERT_TRUE(skewed.complete("100", "200", *skewedToken,
                                SuspiciousState::Monitored, 500));
    EXPECT_EQ(skewed.state("100", "200"), SuspiciousState::Monitored);
}
TEST(SatorinoSuspiciousUsers,
     PendingRequestsAreBoundedAndNeverEvictedToAllowDuplicates)
{
    SuspiciousUserCache cache;
    std::optional<std::uint64_t> first;
    for (std::size_t i = 1; i <= SuspiciousUserCache::PENDING_CAP; ++i)
    {
        const auto token =
            cache.begin("100", QString::number(static_cast<qulonglong>(i)));
        ASSERT_TRUE(token);
        if (i == 1)
            first = token;
    }
    EXPECT_FALSE(cache.begin("100", "9999"));
    ASSERT_TRUE(first);
    EXPECT_TRUE(cache.busy("100", "1"));
    EXPECT_TRUE(cache.complete("100", "1", *first, std::nullopt, 100));
    EXPECT_TRUE(cache.begin("100", "9999"));
    EXPECT_FALSE(cache.begin("100", "bad"));
}
