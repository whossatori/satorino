// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoRaid.hpp"

#include "providers/twitch/eventsub/SubscriptionRequest.hpp"
#include "providers/twitch/eventsub/Controller.hpp"
#include <QDateTime>
#include "Test.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <initializer_list>
#include <unordered_set>

using namespace chatterino::satorino;
namespace {
RaidEvent event(const QString &target = "200", qint64 at = 1000000)
{
    return {"100", target, "target_channel", "Target Channel", at};
}
RaidContext context(qint64 now = 0)
{
    RaidContext result;
    result.enabled = true;
    result.loggedIn = true;
    result.sourceOpen = true;
    result.mainActive = true;
    result.nowMs = now;
    result.nowUtcMs = 1000000 + now;
    result.enabledAtUtcMs = 1000000;
    return result;
}
QJsonObject payload()
{
    return QJsonDocument::fromJson(R"({
        "subscription": {"type":"channel.raid", "version":"1",
            "condition":{"from_broadcaster_user_id":"100"}},
        "event": {"from_broadcaster_user_id":"100", "from_broadcaster_user_login":"source",
            "from_broadcaster_user_name":"Source", "to_broadcaster_user_id":"200",
            "to_broadcaster_user_login":"Target_Channel", "to_broadcaster_user_name":"Target Channel",
            "viewers":42}
    })")
        .object();
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
TEST(SatorinoRaid, ParsesOutgoingSampleAndPreservesMetadataTimestamp)
{
    const auto result = parseRaidEvent(payload(), 1234567);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->sourceId, "100");
    EXPECT_EQ(result->targetId, "200");
    EXPECT_EQ(result->targetLogin, "target_channel");
    EXPECT_EQ(result->targetName, "Target Channel");
    EXPECT_EQ(result->timestampMs, 1234567);
}
TEST(SatorinoRaid, ParserRejectsIncomingWrongVersionAndMalformedPayloads)
{
    EXPECT_FALSE(parseRaidEvent({}, 1234567));
    EXPECT_FALSE(parseRaidEvent(payload(), 0));
    for (const auto &subscription :
         {QJsonObject{
              {"type", "channel.raid"},
              {"version", "2"},
              {"condition", QJsonObject{{"from_broadcaster_user_id", "100"}}}},
          QJsonObject{
              {"type", "channel.raid"},
              {"version", "1"},
              {"condition", QJsonObject{{"to_broadcaster_user_id", "200"}}}},
          QJsonObject{
              {"type", "channel.raid"},
              {"version", "1"},
              {"condition", QJsonObject{{"from_broadcaster_user_id", "999"}}}},
          QJsonObject{
              {"type", "channel.raid"},
              {"version", "1"},
              {"condition", QJsonObject{{"from_broadcaster_user_id", "100"},
                                        {"to_broadcaster_user_id", "200"}}}},
          QJsonObject{
              {"type", "channel.update"},
              {"version", "1"},
              {"condition", QJsonObject{{"from_broadcaster_user_id", "100"}}}}})
    {
        auto sample = payload();
        sample.insert("subscription", subscription);
        EXPECT_FALSE(parseRaidEvent(sample, 1234567));
    }
    auto sample = payload();
    sample.insert("event",
                  QJsonObject{{"from_broadcaster_user_id", 100},
                              {"to_broadcaster_user_id", "200"},
                              {"to_broadcaster_user_login", "target"}});
    EXPECT_FALSE(parseRaidEvent(sample, 1234567));
}
TEST(SatorinoRaid,
     RaidSubscriptionIdentityIncludesOwnerAndRetainsOtherTopicIdentity)
{
    chatterino::eventsub::SubscriptionRequest a;
    a.subscriptionType = "channel.raid";
    a.subscriptionVersion = "1";
    a.ownerTwitchUserID = "1";
    a.conditions.emplace_back("from_broadcaster_user_id", "100");
    auto b = a;
    b.ownerTwitchUserID = "2";
    EXPECT_NE(a, b);
    std::unordered_set<chatterino::eventsub::SubscriptionRequest> requests{a,
                                                                           b};
    EXPECT_EQ(requests.size(), 2U);
    b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<chatterino::eventsub::SubscriptionRequest>{}(a),
              std::hash<chatterino::eventsub::SubscriptionRequest>{}(b));
    a.subscriptionType = "channel.update";
    b = a;
    b.ownerTwitchUserID = "2";
    EXPECT_EQ(a, b);
}

TEST(SatorinoRaid, StatusChooserReportsEveryPipelineGate)
{
    RaidStatusContext status;
    EXPECT_EQ(raidStatusText(status), "Raid watch: off");
    status.enabled = true;
    EXPECT_EQ(raidStatusText(status), "Raid watch: not logged in");
    status.loggedIn = true;
    EXPECT_TRUE(raidStatusText(status).contains("no OAuth token"));
    status.hasToken = true;
    EXPECT_TRUE(raidStatusText(status).contains("enable it and restart"));
    status.eventSubSetting = true;
    EXPECT_TRUE(raidStatusText(status).contains("restart needed"));
    status.eventSubAvailable = true;
    EXPECT_TRUE(raidStatusText(status).contains("no open Twitch"));
    status.waitingForIds = true;
    EXPECT_TRUE(raidStatusText(status).contains("room ID"));
    status.watches.push_back({"own", {RaidWatchState::Waiting, {}}, 0});
    EXPECT_TRUE(raidStatusText(status).contains("waiting for connection"));
    status.watches.front().subscription.state = RaidWatchState::Subscribing;
    EXPECT_TRUE(raidStatusText(status).contains("own: subscribing"));
    status.watches.front().subscription = {RaidWatchState::Subscribed, {}};
    EXPECT_TRUE(raidStatusText(status).contains("watching 1/1"));
    status.watches.front().subscription = {
        RaidWatchState::Failed,
        "HTTP 403: subscription missing proper authorization"};
    status.watches.front().retrySeconds = 5;
    EXPECT_TRUE(raidStatusText(status).contains("could not watch own"));
    EXPECT_TRUE(raidStatusText(status).contains("HTTP 403"));
    EXPECT_TRUE(raidStatusText(status).contains("retry in 5s"));
    status.lastEvent =
        "Last raid event: own -> alt at 12:00, ignored (target is blocked)";
    EXPECT_TRUE(raidStatusText(status).contains(status.lastEvent));
}
TEST(SatorinoRaid, PerWatchStatesAndDetailsIncludePendingSubscribedAndFailure)
{
    EXPECT_EQ(raidWatchStateText({RaidWatchState::Inactive, {}}),
              "not requested");
    EXPECT_EQ(raidWatchStateText({RaidWatchState::Waiting, {}}),
              "waiting for connection");
    EXPECT_TRUE(raidWatchStateText({RaidWatchState::Waiting, "HTTP 500: busy"})
                    .contains("retrying: HTTP 500"));
    EXPECT_EQ(raidWatchStateText({RaidWatchState::Subscribing, {}}),
              "subscribing");
    EXPECT_EQ(raidWatchStateText({RaidWatchState::Subscribed, {}}),
              "subscribed");
    EXPECT_TRUE(raidWatchStateText({RaidWatchState::Failed, {}})
                    .contains("request failed"));
    RaidStatusContext status;
    status.enabled = status.loggedIn = status.hasToken =
        status.eventSubAvailable = status.eventSubSetting = true;
    for (int i = 0; i < 10; ++i)
        status.watches.push_back(
            {QString("channel%1").arg(i), {RaidWatchState::Subscribed, {}}, 0});
    EXPECT_TRUE(raidStatusText(status).contains("watching 10/10"));
    EXPECT_TRUE(raidStatusDetails(status).contains("channel9: subscribed"));
    status.eventSubSetting = false;
    EXPECT_TRUE(raidStatusText(status).contains("restart pending"));
}
TEST(SatorinoRaid, DummyControllerCannotPretendToWatch)
{
    chatterino::eventsub::DummyController controller;
    chatterino::eventsub::SubscriptionRequest request;
    request.subscriptionType = "channel.raid";
    EXPECT_FALSE(controller.raidsAvailable());
    EXPECT_EQ(controller.raidSubscriptionStatus(request).state,
              RaidWatchState::Inactive);
    EXPECT_FALSE(controller.subscribe(request));
}
TEST(SatorinoRaid, FailureBackoffIsBounded)
{
    EXPECT_EQ(raidRetrySeconds(0), 5);
    EXPECT_EQ(raidRetrySeconds(1), 10);
    EXPECT_EQ(raidRetrySeconds(2), 20);
    EXPECT_EQ(raidRetrySeconds(3), 40);
    EXPECT_EQ(raidRetrySeconds(4), 60);
    EXPECT_EQ(raidRetrySeconds(100), 60);
    EXPECT_EQ(raidRetrySeconds(-1), 5);
}
TEST(SatorinoRaid, ReasonsAreRedactedAndBoundedBeforeUiOrLogs)
{
    const auto reason = raidSafeReason(
        "HTTP 401: Bearer my_secret https://example.invalid/?token=my_secret",
        {"my_secret"});
    EXPECT_TRUE(reason.contains("HTTP 401"));
    EXPECT_FALSE(reason.contains("my_secret"));
    EXPECT_FALSE(reason.contains("example.invalid"));
    EXPECT_FALSE(raidSafeReason("HTTP 400: {raw_payload: true}")
                     .contains("raw_payload"));
    EXPECT_LE(raidSafeReason(QString(500, 'x')).size(), 160);
}
TEST(SatorinoRaid, FullMessageAndPayloadOnlyProduceTheSameEvent)
{
    const auto now =
        QDateTime::fromString("2026-11-05T13:45:12.123Z", Qt::ISODateWithMs)
            .toMSecsSinceEpoch();
    ASSERT_GT(now, 0);
    const QJsonObject full{
        {"metadata",
         QJsonObject{{"message_id", "f8d8b9e9-4255-4f31-a404-6c8dbf0ff343"},
                     {"message_type", "notification"},
                     {"message_timestamp", "2026-11-05T13:45:12.123456789Z"},
                     {"subscription_type", "channel.raid"},
                     {"subscription_version", "1"}}},
        {"payload", payload()}};
    QString error;
    const auto bare = parseRaidEvent(payload(), now, &error);
    EXPECT_TRUE(error.isEmpty());
    const auto wrapped = parseRaidEvent(full, now, &error);
    ASSERT_TRUE(bare);
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(wrapped->sourceId, bare->sourceId);
    EXPECT_EQ(wrapped->targetId, bare->targetId);
    EXPECT_EQ(wrapped->timestampMs, now);
    auto malformed = full;
    malformed.insert("metadata", QJsonObject{{"message_type", "revocation"}});
    EXPECT_FALSE(parseRaidEvent(malformed, now, &error));
    EXPECT_EQ(error, "invalid notification metadata");
}
TEST(SatorinoRaid, RealisticUtcMetadataAgeAndLocalArrivalEpoch)
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
