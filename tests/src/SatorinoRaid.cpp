// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoRaid.hpp"
#include "Test.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <set>

using namespace chatterino::satorino;
namespace {
RaidEvent event(const QString &target = "200", qint64 at = 1000000)
{
    return {"100", target, "target_channel", "Target Channel", at};
}
RaidContext context(qint64 now = 0)
{
    RaidContext input;
    input.enabled = input.loggedIn = input.sourceOpen = input.mainActive = true;
    input.nowMs = now;
    input.nowUtcMs = 1000000 + now;
    input.enabledAtUtcMs = 1000000;
    return input;
}
QJsonObject payload(const QString &type = "raid_go_v2",
                    const QString &id = "raid-first",
                    const QString &target = "200")
{
    return QJsonObject{
        {"type", type},
        {"topic", "raid.100"},
        {"raid", QJsonObject{{"id", id},
                             {"source_id", "100"},
                             {"target_id", target},
                             {"target_login", "Target_Channel"},
                             {"target_display_name", "Target Channel"},
                             {"force_raid_now_seconds", 90},
                             {"viewer_count", 275}}}};
}
}  // namespace
TEST(SatorinoRaid,
     DecisionTableAllSettingAccountSourcePlatformFocusCombinations)
{
    for (bool enabled : {false, true})
        for (bool switchTab : {false, true})
            for (bool existing : {false, true})
                for (bool loggedIn : {false, true})
                    for (bool twitch : {false, true})
                        for (bool open : {false, true})
                            for (bool blocked : {false, true})
                                for (bool active : {false, true})
                                    for (bool draft : {false, true})
                                    {
                                        RaidPolicy policy;
                                        auto input = context();
                                        input.enabled = enabled;
                                        input.switchToTab = switchTab;
                                        input.targetExists = existing;
                                        input.loggedIn = loggedIn;
                                        input.twitch = twitch;
                                        input.sourceOpen = open;
                                        input.targetBlocked = blocked;
                                        input.mainActive = active;
                                        input.hasDraft = draft;
                                        const auto result =
                                            policy.decide(event(), input);
                                        const bool allowed =
                                            enabled && loggedIn && twitch &&
                                            open && !blocked;
                                        EXPECT_EQ(
                                            result.action,
                                            !allowed ? RaidAction::Ignore
                                            : existing
                                                ? RaidAction::SelectExisting
                                                : RaidAction::OpenAndJoin);
                                        EXPECT_EQ(result.switchToTab,
                                                  allowed && switchTab &&
                                                      active && !draft);
                                    }
}
TEST(SatorinoRaid, RejectsSelfInvalidIdsAndInvalidLogins)
{
    for (const auto &id :
         {QString{}, QString("0"), QString("01"), QString("kick:200"),
          QString("-5"), QString(33, '1')})
    {
        RaidPolicy policy;
        auto invalid = event(id);
        EXPECT_EQ(policy.decide(invalid, context()).action, RaidAction::Ignore);
        invalid = event();
        invalid.sourceId = id;
        EXPECT_EQ(policy.decide(invalid, context()).action, RaidAction::Ignore);
    }
    for (const auto &login :
         {QString{}, QString("a/b"), QString("https://twitch.tv/target"),
          QString("UPPER"), QString(26, 'a')})
    {
        RaidPolicy policy;
        auto invalid = event();
        invalid.targetLogin = login;
        EXPECT_EQ(policy.decide(invalid, context()).action, RaidAction::Ignore);
    }
    RaidPolicy policy;
    EXPECT_EQ(policy.decide(event("100"), context()).action,
              RaidAction::Ignore);
}
TEST(SatorinoRaid, DedupeIsBySourceTargetAndExpiresAtThreeMinutes)
{
    RaidPolicy policy;
    auto input = context();
    input.targetExists = true;
    EXPECT_EQ(policy.decide(event(), input).action, RaidAction::SelectExisting);
    input.nowMs = RAID_DEDUPE_MS - 1;
    input.nowUtcMs += input.nowMs;
    EXPECT_EQ(policy.decide(event("200", input.nowUtcMs), input).action,
              RaidAction::Ignore);
    input.nowMs = RAID_DEDUPE_MS;
    ++input.nowUtcMs;
    EXPECT_EQ(policy.decide(event("200", input.nowUtcMs), input).action,
              RaidAction::SelectExisting);
    auto otherSource = event("200", input.nowUtcMs);
    otherSource.sourceId = "101";
    EXPECT_EQ(policy.decide(otherSource, input).action,
              RaidAction::SelectExisting);
}
TEST(SatorinoRaid, OneNewTabPerFifteenSecondsButExistingTabsStaySelectable)
{
    RaidPolicy policy;
    EXPECT_EQ(policy.decide(event(), context()).action,
              RaidAction::OpenAndJoin);
    auto input = context(RAID_NEW_TAB_GAP_MS - 1);
    EXPECT_EQ(policy.decide(event("201", input.nowUtcMs), input).action,
              RaidAction::Ignore);
    input.targetExists = true;
    EXPECT_EQ(policy.decide(event("202", input.nowUtcMs), input).action,
              RaidAction::SelectExisting);
    input = context(RAID_NEW_TAB_GAP_MS);
    EXPECT_EQ(policy.decide(event("201", input.nowUtcMs), input).action,
              RaidAction::OpenAndJoin);
}
TEST(SatorinoRaid, FiveNewTabsPerTenMinutesAndCapWindowExpires)
{
    RaidPolicy policy;
    for (int i = 0; i < 5; ++i)
    {
        const auto input = context(i * RAID_NEW_TAB_GAP_MS);
        EXPECT_EQ(
            policy
                .decide(event(QString::number(200 + i), input.nowUtcMs), input)
                .action,
            RaidAction::OpenAndJoin);
    }
    auto input = context(5 * RAID_NEW_TAB_GAP_MS);
    EXPECT_EQ(policy.decide(event("205", input.nowUtcMs), input).action,
              RaidAction::Ignore);
    input = context(RAID_CAP_WINDOW_MS - 1);
    EXPECT_EQ(policy.decide(event("205", input.nowUtcMs), input).action,
              RaidAction::Ignore);
    input = context(RAID_CAP_WINDOW_MS);
    EXPECT_EQ(policy.decide(event("205", input.nowUtcMs), input).action,
              RaidAction::OpenAndJoin);
}
TEST(SatorinoRaid, IgnoredEventsDoNotConsumeDedupeOrRateBudget)
{
    RaidPolicy policy;
    auto input = context();
    input.enabled = false;
    EXPECT_EQ(policy.decide(event(), input).action, RaidAction::Ignore);
    input.enabled = true;
    EXPECT_EQ(policy.decide(event(), input).action, RaidAction::OpenAndJoin);
}
TEST(SatorinoRaid, RejectsStaleFutureAndPriorEnableEpochNotifications)
{
    for (qint64 at : {qint64(0), qint64(999999), qint64(1030001)})
    {
        RaidPolicy policy;
        EXPECT_EQ(policy.decide(event("200", at), context()).action,
                  RaidAction::Ignore);
    }
    RaidPolicy policy;
    auto input = context(RAID_DEDUPE_MS + 1);
    EXPECT_EQ(policy.decide(event(), input).action, RaidAction::Ignore);
}
TEST(SatorinoRaid, FeedReceiptAgeAndLocalArrivalEpoch)
{
    const auto now =
        QDateTime::fromString("2026-11-05T13:45:12.123Z", Qt::ISODateWithMs)
            .toMSecsSinceEpoch();
    RaidContext input = context();
    input.nowUtcMs = now;
    input.enabledAtUtcMs = now - 1000;
    input.receivedAtUtcMs = now;
    // Server time slightly before local enable time is legitimate when local arrival is current.
    RaidPolicy policy;
    EXPECT_EQ(policy.decide(event("200", now - 5000), input).action,
              RaidAction::OpenAndJoin);
    RaidPolicy queued;
    input.receivedAtUtcMs = input.enabledAtUtcMs - 1;
    EXPECT_EQ(queued.decide(event("201", now), input).reason,
              "received before watch started");
    input.receivedAtUtcMs = now;
    RaidPolicy stale;
    EXPECT_TRUE(stale.decide(event("202", now - RAID_DEDUPE_MS - 1), input)
                    .reason.contains("older than three minutes"));
    RaidPolicy future;
    EXPECT_TRUE(future.decide(event("203", now + 30001), input)
                    .reason.contains("future"));
}
TEST(SatorinoRaid, DecisionReasonsExplainIgnoredAndUnswitchedActions)
{
    RaidPolicy policy;
    auto input = context();
    input.targetBlocked = true;
    EXPECT_EQ(policy.decide(event(), input).reason, "target is blocked");
    input.targetBlocked = false;
    input.hasDraft = true;
    EXPECT_EQ(policy.decide(event(), input).reason,
              "current input has a draft");
    EXPECT_TRUE(policy.decide(event(), input).reason.contains("duplicate"));
    input.nowMs = 1000;
    EXPECT_TRUE(
        policy.decide(event("201"), input).reason.contains("rate limit"));
}

TEST(SatorinoRaid, ExistingFeedUpdateAndGoPayloadShapesMapTargetAndViewers)
{
    for (const auto &type : {QString("raid_update_v2"), QString("raid_go_v2")})
    {
        const auto raw = payload(type);
        const auto wrapped = QJsonObject{
            {"type", type}, {"data", QJsonObject{{"raid", raw.value("raid")}}}};
        for (const auto &shape : {raw, wrapped})
        {
            const auto normalized = normaliseRaidFeed("raid.100", shape);
            ASSERT_TRUE(normalized);
            const auto feed = parseRaidFeed(*normalized, 1000000);
            ASSERT_TRUE(feed);
            ASSERT_TRUE(feed->event);
            EXPECT_EQ(feed->kind, type == "raid_go_v2" ? RaidFeedKind::Go
                                                       : RaidFeedKind::Update);
            EXPECT_EQ(feed->sourceId, "100");
            EXPECT_EQ(feed->event->targetId, "200");
            EXPECT_EQ(feed->event->targetLogin, "target_channel");
            EXPECT_EQ(feed->event->targetName, "Target Channel");
            EXPECT_EQ(feed->event->viewerCount, 275);
            EXPECT_EQ(feed->event->timestampMs, 1000000);
        }
    }
}
TEST(SatorinoRaid, ExistingCamelCaseAliasesAndStringCountsAreAccepted)
{
    auto input = payload();
    input.insert("raid", QJsonObject{{"id", "raid-first"},
                                     {"sourceID", "100"},
                                     {"targetID", "200"},
                                     {"targetLogin", "target_channel"},
                                     {"targetDisplayName", "Target"},
                                     {"viewerCount", "42"},
                                     {"forceRaidNowSeconds", 90}});
    const auto parsed = parseRaidEvent(input, 1000000);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->viewerCount, 42);
    EXPECT_EQ(parsed->targetName, "Target");
}
TEST(SatorinoRaid,
     BareGoUsesMatchingCountdownTargetAndCancelCanHaveNoRaidObject)
{
    RaidFeedTracker feed;
    EXPECT_EQ(feed.observe(payload("raid_update_v2"), 1000000).action,
              RaidFeedAction::Wait);
    const QJsonObject go{{"type", "raid_go_v2"},
                         {"data", QJsonObject{{"raid_id", "raid-first"}}}};
    const auto normalized = normaliseRaidFeed("raid.100", go);
    ASSERT_TRUE(normalized);
    const auto result = feed.observe(*normalized, 1001000);
    EXPECT_EQ(result.action, RaidFeedAction::Fire);
    ASSERT_TRUE(result.event);
    EXPECT_EQ(result.event->targetLogin, "target_channel");
    EXPECT_EQ(result.event->viewerCount, 275);
    EXPECT_EQ(result.event->timestampMs, 1001000);
    const auto cancel =
        normaliseRaidFeed("raid.100", QJsonObject{{"type", "raid_cancel_v2"}});
    ASSERT_TRUE(cancel);
    const auto cancelled = parseRaidFeed(*cancel, 1002000);
    ASSERT_TRUE(cancelled);
    EXPECT_EQ(cancelled->kind, RaidFeedKind::Cancel);
}
TEST(SatorinoRaid, OnlyGoFiresDuringCountdownAndDuplicateGoCannotFireAgain)
{
    RaidFeedTracker feed;
    EXPECT_EQ(feed.observe(payload("raid_update_v2"), 1000000).action,
              RaidFeedAction::Wait);
    EXPECT_EQ(feed.observe(payload("raid_update_v2"), 1001000).action,
              RaidFeedAction::Wait);
    EXPECT_EQ(feed.observe(payload(), 1005000).action, RaidFeedAction::Fire);
    EXPECT_EQ(feed.observe(payload(), 1005001).action, RaidFeedAction::Ignore);
    EXPECT_EQ(feed.observe(payload("raid_update_v2"), 1006000).action,
              RaidFeedAction::Ignore);
    EXPECT_TRUE(feed.expire(1200000).isEmpty());
}
TEST(SatorinoRaid, CancelNeverOpensEvenWithLateGoOrLateUpdates)
{
    RaidFeedTracker feed;
    feed.observe(payload("raid_update_v2"), 1000000);
    EXPECT_EQ(feed.observe(payload("raid_cancel_v2"), 1001000).action,
              RaidFeedAction::Cancelled);
    EXPECT_FALSE(feed.nextDeadline());
    EXPECT_EQ(feed.observe(payload(), 1002000).action, RaidFeedAction::Ignore);
    EXPECT_EQ(feed.observe(payload("raid_update_v2"), 1003000).action,
              RaidFeedAction::Ignore);
    EXPECT_TRUE(feed.expire(1200000).isEmpty());
}
TEST(SatorinoRaid,
     ReorderedCancelBeforeUpdateIsRememberedAndOldCancelDoesNotStopNewRaid)
{
    RaidFeedTracker feed;
    feed.observe(payload("raid_cancel_v2", "raid-first"), 1000000);
    EXPECT_EQ(
        feed.observe(payload("raid_update_v2", "raid-first"), 1001000).action,
        RaidFeedAction::Ignore);
    EXPECT_EQ(
        feed.observe(payload("raid_update_v2", "raid-second"), 1002000).action,
        RaidFeedAction::Wait);
    feed.observe(payload("raid_cancel_v2", "raid-first"), 1003000);
    EXPECT_EQ(
        feed.observe(payload("raid_go_v2", "raid-second"), 1004000).action,
        RaidFeedAction::Fire);
    RaidFeedTracker previous;
    previous.observe(payload("raid_go_v2", "old"), 1000000);
    previous.observe(payload("raid_cancel_v2", "new"), 1001000);
    EXPECT_EQ(previous.observe(payload("raid_go_v2", "new"), 1002000).action,
              RaidFeedAction::Ignore);
}
TEST(SatorinoRaid,
     CountdownFallbackExpiresOnceAndRepeatedUpdatesDoNotSlideDeadline)
{
    RaidFeedTracker feed;
    feed.observe(payload("raid_update_v2"), 1000000);
    ASSERT_TRUE(feed.nextDeadline());
    EXPECT_EQ(*feed.nextDeadline(), 1090000);
    feed.observe(payload("raid_update_v2"), 1020000);
    EXPECT_EQ(*feed.nextDeadline(), 1090000);
    EXPECT_TRUE(feed.expire(1089999).isEmpty());
    const auto result = feed.expire(1090000);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().reason, "countdown end");
    EXPECT_EQ(result.front().event->timestampMs, 1090000);
    EXPECT_TRUE(feed.expire(1100000).isEmpty());
    EXPECT_EQ(feed.observe(payload(), 1091000).action, RaidFeedAction::Ignore);
}
TEST(SatorinoRaid,
     AbsoluteCountdownUsesTheSameCreatedAtAndForceDurationAsTheBar)
{
    const auto now = QDateTime::fromString("2026-11-05T12:00:00Z", Qt::ISODate)
                         .toMSecsSinceEpoch();
    auto input = payload("raid_update_v2");
    auto raid = input.value("raid").toObject();
    raid.insert("raid_created_at", QString::number(now / 1000 - 30));
    input.insert("raid", raid);
    RaidFeedTracker feed;
    feed.observe(input, now);
    ASSERT_TRUE(feed.nextDeadline());
    EXPECT_EQ(*feed.nextDeadline(), now + 60000);
}
TEST(SatorinoRaid,
     ClosedSourcesArePrunedAndMismatchedGoCannotOpenTheCachedTarget)
{
    RaidFeedTracker feed;
    feed.observe(payload("raid_update_v2"), 1000000);
    feed.retain(std::set<QString>{"101"});
    EXPECT_FALSE(feed.nextDeadline());
    EXPECT_TRUE(feed.expire(1200000).isEmpty());
    feed.observe(payload("raid_update_v2"), 1000000);
    const QJsonObject mismatch{
        {"type", "raid_go_v2"},
        {"topic", "raid.100"},
        {"raid", QJsonObject{{"id", "raid-first"}, {"target_id", "201"}}}};
    const auto result = feed.observe(mismatch, 1001000);
    EXPECT_EQ(result.action, RaidFeedAction::Ignore);
    EXPECT_TRUE(result.reason.contains("does not match"));
    EXPECT_FALSE(feed.nextDeadline());
}
TEST(SatorinoRaid, MalformedFeedAndSourceMismatchAreRejected)
{
    EXPECT_FALSE(
        normaliseRaidFeed("raid.100", QJsonObject{{"type", "raid_unknown"}}));
    EXPECT_FALSE(
        normaliseRaidFeed("raid.100", QJsonObject{{"type", "raid_update_v2"}}));
    QString error;
    EXPECT_FALSE(parseRaidFeed({}, 1000000, &error));
    EXPECT_FALSE(error.isEmpty());
    auto input = payload();
    input.insert("topic", "raid.bad");
    EXPECT_FALSE(parseRaidFeed(input, 1000000));
    input = payload();
    auto raid = input.value("raid").toObject();
    raid.insert("source_id", "101");
    input.insert("raid", raid);
    EXPECT_FALSE(parseRaidFeed(input, 1000000));
    input = payload();
    raid = input.value("raid").toObject();
    raid.insert("target_login", "bad/login");
    input.insert("raid", raid);
    EXPECT_FALSE(parseRaidFeed(input, 1000000));
}
TEST(SatorinoRaid, SharedListeningConditionUsesEitherOption)
{
    EXPECT_FALSE(raidListeningEnabled(false, false));
    EXPECT_TRUE(raidListeningEnabled(true, false));
    EXPECT_TRUE(raidListeningEnabled(false, true));
    EXPECT_TRUE(raidListeningEnabled(true, true));
}
TEST(SatorinoRaid, SharedFeedStatusHasNoTenChannelLimitOrEventSubDependency)
{
    RaidStatusContext status;
    EXPECT_EQ(raidStatusText(status), "Raid watch: off");
    status.enabled = true;
    EXPECT_EQ(raidStatusText(status), "Raid watch: not logged in");
    status.loggedIn = true;
    status.listening = 37;
    status.waitingForIds = 2;
    EXPECT_TRUE(raidStatusText(status).contains("listening on 37"));
    EXPECT_TRUE(raidStatusText(status).contains("room ID"));
    EXPECT_TRUE(raidStatusText(status).contains("no raid seen yet"));
    EXPECT_FALSE(raidStatusText(status).contains("EventSub"));
    status.lastEvent = "Last raid: own -> alt at 12:00, cancelled";
    EXPECT_TRUE(raidStatusText(status).contains(status.lastEvent));
    EXPECT_TRUE(raidStatusDetails(status).contains("raid_go_v2"));
    EXPECT_TRUE(raidStatusDetails(status).contains("countdown end"));
    EXPECT_TRUE(raidStatusDetails(status).contains("raid bar"));
}
TEST(SatorinoRaid, TestCommandParsingAndExplicitIdOverride)
{
    QString error;
    const auto plain =
        parseRaidTestCommand({"/satorino-testraid", "Alt_Login"}, &error);
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->login, "alt_login");
    EXPECT_TRUE(plain->id.isEmpty());
    EXPECT_TRUE(raidTestKnownId(*plain, {}).isEmpty());
    EXPECT_EQ(raidTestKnownId(*plain, "200"), "200");
    const auto explicitId =
        parseRaidTestCommand({"/satorino-testraid", "alt", "201"});
    ASSERT_TRUE(explicitId);
    EXPECT_EQ(raidTestKnownId(*explicitId, "200"), "201");
    EXPECT_FALSE(parseRaidTestCommand({"/satorino-testraid"}));
    EXPECT_FALSE(
        parseRaidTestCommand({"/satorino-testraid", "https://twitch.tv/alt"}));
    EXPECT_FALSE(
        parseRaidTestCommand({"/satorino-testraid", "alt", "invalid"}));
    EXPECT_FALSE(
        parseRaidTestCommand({"/satorino-testraid", "alt", "200", "extra"}));
}
TEST(SatorinoRaid, ExplicitCommandLookupHandlesFoundNotFoundErrorAndTimeout)
{
    const auto found =
        raidLookupResult("alt", RaidLookupStatus::Found, "200", "ALT");
    EXPECT_TRUE(found.usable);
    EXPECT_EQ(found.id, "200");
    EXPECT_FALSE(
        raidLookupResult("alt", RaidLookupStatus::Found, "0", "alt").usable);
    EXPECT_FALSE(
        raidLookupResult("alt", RaidLookupStatus::Found, "200", "other")
            .usable);
    EXPECT_EQ(raidLookupResult("alt", RaidLookupStatus::NotFound).reason,
              "target not found");
    EXPECT_EQ(raidLookupResult("alt", RaidLookupStatus::Error).reason,
              "users lookup failed");
    EXPECT_EQ(raidLookupResult("alt", RaidLookupStatus::Timeout).reason,
              "users lookup timed out");
}
TEST(SatorinoRaid, GoAndCountdownOutputsStillUseTheExistingRateCaps)
{
    RaidFeedTracker feed;
    RaidPolicy policy;
    auto first = feed.observe(payload(), 1000000);
    ASSERT_TRUE(first.event);
    EXPECT_EQ(policy.decide(*first.event, context()).action,
              RaidAction::OpenAndJoin);
    auto burst =
        feed.observe(payload("raid_go_v2", "raid-next", "201"), 1001000);
    ASSERT_TRUE(burst.event);
    EXPECT_EQ(policy.decide(*burst.event, context(1000)).action,
              RaidAction::Ignore);
}

TEST(SatorinoRaid, BareCancelRemainsCancelledAfterAnotherRaidStarts)
{
    RaidFeedTracker feed;
    feed.observe(payload("raid_update_v2"), 1000000);
    feed.observe(QJsonObject{{"type", "raid_cancel_v2"}, {"topic", "raid.100"}},
                 1001000);
    feed.observe(payload("raid_update_v2", "new"), 1002000);
    EXPECT_EQ(feed.observe(payload(), 1003000).action, RaidFeedAction::Ignore);
}
TEST(SatorinoRaid, UnreasonableCountdownCannotCauseAnEarlyFallback)
{
    RaidFeedTracker feed;
    auto input = payload("raid_update_v2");
    auto raid = input.value("raid").toObject();
    raid.insert("force_raid_now_seconds", 5000000);
    input.insert("raid", raid);
    EXPECT_EQ(feed.observe(input, 1000000).action, RaidFeedAction::Wait);
    EXPECT_FALSE(feed.nextDeadline());
    EXPECT_TRUE(feed.expire(2000000).isEmpty());
    EXPECT_EQ(feed.observe(payload(), 1001000).action, RaidFeedAction::Fire);
}

TEST(SatorinoRaid, SubmittedInputIsScopedAndDifferentDraftAlwaysBlocks)
{
    EXPECT_FALSE(raidHasUnsentDraft("", "/raidsend"));
    EXPECT_FALSE(raidHasUnsentDraft("/raidsend", "/raidsend"));
    EXPECT_FALSE(raidHasUnsentDraft("/alias-send", "/alias-send"));
    EXPECT_TRUE(raidHasUnsentDraft("a real draft", "/raidsend"));
    EXPECT_TRUE(raidHasUnsentDraft(
        "/raidsend"));  // An unsent command has no exemption.
    EXPECT_TRUE(raidHasUnsentDraft("/raidsend ", "/raidsend"));
    EXPECT_TRUE(
        raidCanSwitch(true, raidHasUnsentDraft("/raidsend", "/raidsend")));
    EXPECT_FALSE(
        raidCanSwitch(true, raidHasUnsentDraft("new draft", "/raidsend")));
    EXPECT_FALSE(
        raidCanSwitch(false, false));  // Another application owns focus.
}
TEST(SatorinoRaid, EarlyLocalSuccessThenGoAndExpiryFireOnlyOnce)
{
    for (bool successFirst : {false, true})
    {
        RaidFeedTracker tracker;
        EXPECT_EQ(tracker.observe(payload("raid_update_v2"), 1000000).action,
                  RaidFeedAction::Wait);
        const auto localSuccess =
            payload();  // clearActiveRaid(true) maps saved target to go.
        const auto remoteGo = payload();
        EXPECT_EQ(
            tracker.observe(successFirst ? localSuccess : remoteGo, 1001000)
                .action,
            RaidFeedAction::Fire);
        EXPECT_EQ(
            tracker.observe(successFirst ? remoteGo : localSuccess, 1001001)
                .action,
            RaidFeedAction::Ignore);
        EXPECT_TRUE(tracker.expire(1100000).isEmpty());
    }
}
TEST(SatorinoRaid,
     SwitchDecisionReadsCurrentInputAfterSubmissionAndAtActionTime)
{
    // The queued early send runs after input clearing, not with a countdown-time snapshot.
    auto current = context();
    current.hasDraft = raidHasUnsentDraft("", "");
    RaidPolicy early;
    EXPECT_TRUE(early.decide(event(), current).switchToTab);
    current.hasDraft = raidHasUnsentDraft("a new unsent message", "");
    RaidPolicy different;
    EXPECT_FALSE(different.decide(event(), current).switchToTab);
    EXPECT_FALSE(raidCanSwitch(current.mainActive, current.hasDraft));
    current.mainActive = false;
    current.hasDraft = false;
    RaidPolicy external;
    EXPECT_FALSE(external.decide(event(), current).switchToTab);
}
