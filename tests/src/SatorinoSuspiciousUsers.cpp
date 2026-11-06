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
        EXPECT_FALSE(flagControlVisible(true, true, role, true));
    EXPECT_TRUE(flagControlVisible(true, true, FlagTargetRole::Regular,
                                   true));  // Moderator or broadcaster account.
    EXPECT_FALSE(flagControlVisible(true, false, FlagTargetRole::Regular,
                                    true));  // Viewer.
    EXPECT_FALSE(flagControlVisible(false, true, FlagTargetRole::Regular,
                                    true));  // Kick.
    EXPECT_FALSE(flagControlVisible(false, false, FlagTargetRole::Regular,
                                    true));  // YouTube/viewer.
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

TEST(SatorinoSuspiciousUsers,
     NativePresentationDefaultsAreDistinctAndPreserveAlpha)
{
    EXPECT_TRUE(NATIVE_FLAGS_ENABLED_DEFAULT);
    EXPECT_EQ(QColor(NATIVE_MONITORED_COLOUR_DEFAULT),
              QColor(255, 149, 0, 102));
    EXPECT_EQ(QColor(NATIVE_RESTRICTED_COLOUR_DEFAULT),
              QColor(255, 59, 48, 102));
    const QColor monitored(10, 20, 30, 47), restricted(40, 50, 60, 78);
    EXPECT_EQ(nativeFlagStyle(SuspiciousState::Monitored, true, true, false,
                              false, monitored, restricted)
                  .background,
              monitored);
    EXPECT_EQ(nativeFlagStyle(SuspiciousState::Restricted, true, true, false,
                              false, monitored, restricted)
                  .background,
              restricted);
    EXPECT_EQ(nativeFlagStyle(SuspiciousState::Monitored, true, true, false,
                              false, QColor{}, QColor{})
                  .background,
              QColor(NATIVE_MONITORED_COLOUR_DEFAULT));
}
TEST(SatorinoSuspiciousUsers, NativeMarkColourPriorityAndIgnoreDecisionTable)
{
    for (const auto state :
         {SuspiciousState::Unknown, SuspiciousState::None,
          SuspiciousState::Monitored, SuspiciousState::Restricted})
        for (bool enabled : {false, true})
            for (bool ordinary : {false, true})
                for (bool protectedHighlight :
                     {false,
                      true})  // Mentions/redeems and warning/system styles.
                    for (bool ignore : {false, true})
                    {
                        const auto style = nativeFlagStyle(
                            state, enabled, ordinary, protectedHighlight,
                            ignore, QColor(NATIVE_MONITORED_COLOUR_DEFAULT),
                            QColor(NATIVE_RESTRICTED_COLOUR_DEFAULT));
                        const bool marked =
                            enabled && ordinary &&
                            (state == SuspiciousState::Monitored ||
                             state == SuspiciousState::Restricted);
                        EXPECT_EQ(style.mark, marked);
                        EXPECT_EQ(style.background.has_value(),
                                  marked && !protectedHighlight && !ignore);
                        if (style.background)
                            EXPECT_EQ(
                                *style.background,
                                QColor(state == SuspiciousState::Restricted
                                           ? NATIVE_RESTRICTED_COLOUR_DEFAULT
                                           : NATIVE_MONITORED_COLOUR_DEFAULT));
                    }
}
TEST(SatorinoSuspiciousUsers,
     MasterOffHidesAllRolesStatesAndRestoresKnownStateOnEnable)
{
    for (const auto state :
         {SuspiciousState::Unknown, SuspiciousState::None,
          SuspiciousState::Monitored, SuspiciousState::Restricted})
        for (const auto role :
             {FlagTargetRole::Unknown, FlagTargetRole::Regular,
              FlagTargetRole::Self, FlagTargetRole::Moderator,
              FlagTargetRole::Broadcaster})
            for (bool real : {false, true})
                for (bool moderate : {false, true})
                {
                    EXPECT_FALSE(
                        flagControlVisible(real, moderate, role, false));
                    const auto off = nativeFlagStyle(state, false, true, false,
                                                     false, QColor{}, QColor{});
                    EXPECT_FALSE(off.mark);
                    EXPECT_FALSE(off.background);
                    EXPECT_EQ(
                        flagControlVisible(real, moderate, role, true),
                        real && moderate && role == FlagTargetRole::Regular);
                }
    SuspiciousUserCache cache;
    ASSERT_TRUE(cache.observe("100", "200", SuspiciousState::Monitored, 100));
    EXPECT_FALSE(nativeFlagStyle(cache.state("100", "200"), false, true, false,
                                 false, QColor{}, QColor{})
                     .mark);
    EXPECT_TRUE(nativeFlagStyle(cache.state("100", "200"), true, true, false,
                                false, QColor{}, QColor{})
                    .mark);
}
TEST(SatorinoSuspiciousUsers,
     ExplicitNoneClearsStaleOwnFlagsAndUnknownUsesPositiveOwnFlags)
{
    EXPECT_EQ(nativeFlagState(SuspiciousState::Unknown, false, false),
              SuspiciousState::Unknown);
    EXPECT_EQ(nativeFlagState(SuspiciousState::Unknown, true, false),
              SuspiciousState::Monitored);
    EXPECT_EQ(nativeFlagState(SuspiciousState::Unknown, true, true),
              SuspiciousState::Restricted);
    EXPECT_EQ(nativeFlagState(SuspiciousState::None, true, true),
              SuspiciousState::None);
    EXPECT_EQ(nativeFlagState(SuspiciousState::Monitored, false, true),
              SuspiciousState::Monitored);
    EXPECT_FALSE(
        nativeFlagStyle(nativeFlagState(SuspiciousState::None, true, true),
                        true, true, false, false, QColor{}, QColor{})
            .mark);
}
TEST(SatorinoSuspiciousUsers,
     SharedLocalRoomKeysIsolateAuthorsAndLearnLateStatus)
{
    const auto room = flagRoomKey("Source_A", "source_a", "100");
    ASSERT_EQ(room, "100");
    EXPECT_TRUE(flagRoomKey("source_a", "source_b", "101").isEmpty());
    EXPECT_TRUE(flagRoomKey("source_a", "source_a", "bad").isEmpty());
    EXPECT_TRUE(flagRoomKey("", "source_a", "100").isEmpty());
    ASSERT_TRUE(flagTarget(room, "200"));
    SuspiciousUserCache cache;
    EXPECT_FALSE(nativeFlagStyle(cache.state(room, "200"), true, true, false,
                                 false, QColor{}, QColor{})
                     .mark);
    // A later feed observation is enough; existing message layouts can use the same key again.
    ASSERT_TRUE(cache.observe(room, "200", SuspiciousState::Restricted, 100));
    EXPECT_TRUE(nativeFlagStyle(cache.state(room, "200"), true, true, false,
                                false, QColor{}, QColor{})
                    .mark);
    EXPECT_FALSE(nativeFlagStyle(cache.state("101", "200"), true, true, false,
                                 false, QColor{}, QColor{})
                     .mark);
    EXPECT_FALSE(nativeFlagStyle(cache.state(room, "201"), true, true, false,
                                 false, QColor{}, QColor{})
                     .mark);
    // The same pure match is used for each real aggregate child and for registry fallback.
    EXPECT_EQ(flagRoomKey("source_b", "source_b", "101"), "101");
    ASSERT_TRUE(cache.observe(room, "200", SuspiciousState::None, 101));
    const auto removed = nativeFlagStyle(cache.state(room, "200"), true, true,
                                         false, false, QColor{}, QColor{});
    EXPECT_FALSE(removed.mark);
    EXPECT_FALSE(removed.background);
}
TEST(SatorinoSuspiciousUsers,
     MentionBoundariesKeepMentionsAboveOrdinaryHighlightColour)
{
    EXPECT_TRUE(nativeFlagMentions("hello @Satori,", "satori"));
    EXPECT_TRUE(nativeFlagMentions("SATORI", "satori"));
    EXPECT_FALSE(
        nativeFlagMentions("satorino and satori2 and not_satori", "satori"));
    EXPECT_FALSE(nativeFlagMentions("text", ""));
    const auto ordinary =
        nativeFlagStyle(SuspiciousState::Monitored, true, true, false, false,
                        QColor{}, QColor{});
    ASSERT_TRUE(ordinary.background);
    EXPECT_NE(*ordinary.background, QColor(FLAGS_COLOUR_DEFAULT));
    const auto mention = nativeFlagStyle(SuspiciousState::Monitored, true, true,
                                         true, false, QColor{}, QColor{});
    EXPECT_TRUE(mention.mark);
    EXPECT_FALSE(mention.background);
}
