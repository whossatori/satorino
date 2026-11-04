// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondData.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"

#include <gtest/gtest.h>

using namespace chatterino::satorino;
namespace {
AutoRespondInput live(QString text = "raffle")
{
    return {"twitch:foo", "someone", std::move(text), true,
            true,         false,     false,           false};
}
AutoRespondTrigger trigger(QString pattern = "raffle")
{
    AutoRespondTrigger r;
    r.id = "rule";
    r.enabled = true;
    r.pattern = std::move(pattern);
    r.response = "!join";
    return r;
}
QDateTime utc(int day, int hour, int minute, int second = 0)
{
    return QDateTime(QDate(2026, 11, day), QTime(hour, minute, second),
                     QTimeZone::UTC);
}
}  // namespace

TEST(AutoRespond, LiteralBoundariesAndCaseMatchHighlights)
{
    auto r = trigger("cat");
    EXPECT_FALSE(matchAutoRespond(r, live("concatenate")));
    EXPECT_TRUE(matchAutoRespond(r, live("the CAT!")));
    r.caseSensitive = true;
    EXPECT_FALSE(matchAutoRespond(r, live("CAT")));
    EXPECT_TRUE(matchAutoRespond(r, live("cat")));
    r.pattern = "[cat]";
    EXPECT_TRUE(matchAutoRespond(r, live(" [cat] ")));
}
TEST(AutoRespond, RegexRaffleExampleCapturesAndInvalidPatterns)
{
    auto r = trigger(R"(^!join\b|raffle)");
    r.regex = true;
    EXPECT_TRUE(matchAutoRespond(r, live("!join please")));
    EXPECT_TRUE(matchAutoRespond(r, live("a raffle is open")));
    EXPECT_FALSE(matchAutoRespond(r, live("!joining")));
    r.pattern = "[";
    EXPECT_FALSE(autoRespondExpression(r).isValid());
    EXPECT_FALSE(matchAutoRespond(r, live()));
}
TEST(AutoRespond, EnabledAndPlatformChannelIsolation)
{
    auto r = trigger();
    r.enabled = false;
    EXPECT_FALSE(matchAutoRespond(r, live()));
    r.enabled = true;
    r.channel = "kick:foo";
    EXPECT_FALSE(matchAutoRespond(r, live()));
    auto input = live();
    input.channel = "kick:foo";
    EXPECT_TRUE(matchAutoRespond(r, input));
    input.channel = "youtube:foo";
    r.channel = "*";
    EXPECT_FALSE(matchAutoRespond(r, input));
    input.channel = "twitch:foo";
    r.pattern.clear();
    EXPECT_FALSE(matchAutoRespond(r, input));
}
TEST(AutoRespond, LiveAccountAndLoopGuards)
{
    const auto r = trigger();
    for (int mode = 0; mode < 5; ++mode)
    {
        auto input = live();
        if (mode == 0)
            input.live = false;
        if (mode == 1)
            input.loggedIn = false;
        if (mode == 2)
            input.anonymous = true;
        if (mode == 3)
            input.own = true;
        if (mode == 4)
            input.autoSent = true;
        EXPECT_FALSE(matchAutoRespond(r, input));
    }
}
TEST(AutoRespond, PlaceholderExpansionIsSinglePass)
{
    auto r = trigger(R"((raffle) (\d+))");
    r.regex = true;
    auto input = live("raffle 42");
    input.user = "{channel}";
    const auto match = matchAutoRespond(r, input);
    ASSERT_TRUE(match);
    EXPECT_EQ(
        expandAutoRespond("{user}|{channel}|{match}|{1}|{2}|{9}|{unknown}",
                          input, *match),
        "{channel}|foo|raffle 42|raffle|42||{unknown}");
}
TEST(AutoRespond, ResponsesAreOneLineAndBoundedWithoutSplittingUnicode)
{
    EXPECT_EQ(sanitizeAutoRespond(QString::fromUtf8("a\r\nb\t\x01\xe2\x80\xa8"
                                                    "c")),
              "a b c");
    EXPECT_EQ(sanitizeAutoRespond(QString(1000, u'x')).size(), 500);
    EXPECT_TRUE(sanitizeAutoRespond("hello", 0).isEmpty());
    const auto emoji = QString::fromUtf8("\xf0\x9f\x98\x80");
    EXPECT_TRUE(sanitizeAutoRespond(emoji, 3).isEmpty());
    EXPECT_EQ(sanitizeAutoRespond(emoji, 4), emoji);
    const auto r = trigger();
    const auto input = live();
    const auto match = matchAutoRespond(r, input);
    ASSERT_TRUE(match);
    EXPECT_LE(expandAutoRespond(QString(1000, u'x'), input, *match, 50)
                  .toUtf8()
                  .size(),
              50);
}
TEST(AutoRespond, RuleCooldownMinimumAndIndependentRules)
{
    AutoRespondLimits gate;
    EXPECT_TRUE(gate.reserveRule("one", 30, 0));
    EXPECT_FALSE(gate.reserveRule("one", 30, 29999));
    EXPECT_TRUE(gate.reserveRule("two", 30, 1));
    EXPECT_TRUE(gate.reserveRule("one", 30, 30000));
    EXPECT_TRUE(gate.reserveRule("min", 0, 0));
    EXPECT_FALSE(gate.reserveRule("min", 0, 999));
    EXPECT_TRUE(gate.reserveRule("min", 0, 1000));
}
TEST(AutoRespond, GlobalCapIsSlidingAcrossRulesAndTimers)
{
    AutoRespondLimits gate;
    for (int i = 0; i < 6; ++i)
        EXPECT_TRUE(gate.takeGlobal(6, i));
    EXPECT_FALSE(gate.takeGlobal(6, 59999));
    EXPECT_TRUE(gate.takeGlobal(6, 60000));
    EXPECT_FALSE(gate.takeGlobal(6, 60000));
    EXPECT_FALSE(gate.takeGlobal(0, 120000));
}
TEST(AutoRespond, EchoGuardIsChannelScopedAndExpires)
{
    AutoRespondLimits gate;
    gate.rememberSent("twitch:foo", "!join", 0);
    EXPECT_TRUE(gate.wasAutoSent("twitch:foo", " !join ", 500));
    EXPECT_FALSE(gate.wasAutoSent("kick:foo", "!join", 500));
    EXPECT_FALSE(gate.wasAutoSent("twitch:foo", "!join", 60000));
}
TEST(AutoRespond, TimerWeekdaysAndAlreadyConsumedDays)
{
    AutoRespondTimer r;
    r.enabled = true;
    r.time = "12:00";
    r.days = 1;
    const auto next = nextAutoRespondFire(r, utc(2, 11, 0), {}, QTimeZone::UTC);
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, utc(2, 12, 0));  // Monday
    EXPECT_EQ(*nextAutoRespondFire(r, utc(2, 13, 0), {}, QTimeZone::UTC),
              utc(9, 12, 0));
    EXPECT_EQ(*nextAutoRespondFire(r, utc(2, 11, 0), QDate(2026, 11, 2),
                                   QTimeZone::UTC),
              utc(9, 12, 0));
    r.days = 0;
    EXPECT_FALSE(nextAutoRespondFire(r, utc(2, 11, 0), {}, QTimeZone::UTC));
}
TEST(AutoRespond, TimerSkipsMissedStartupSuspendAndClockJumps)
{
    AutoRespondTimer r;
    r.enabled = true;
    r.time = "12:00";
    EXPECT_TRUE(autoRespondTimerDue(r, utc(2, 11, 59, 59), utc(2, 12, 0), {},
                                    QTimeZone::UTC));
    EXPECT_FALSE(autoRespondTimerDue(r, utc(2, 11, 58), utc(2, 12, 1), {},
                                     QTimeZone::UTC));
    EXPECT_FALSE(autoRespondTimerDue(r, utc(2, 12, 0), utc(2, 12, 0, 1), {},
                                     QTimeZone::UTC));
    EXPECT_FALSE(autoRespondTimerDue(r, utc(2, 12, 1), utc(2, 11, 59), {},
                                     QTimeZone::UTC));
    EXPECT_FALSE(autoRespondTimerDue(r, utc(2, 11, 59, 59), utc(2, 12, 0),
                                     QDate(2026, 11, 2), QTimeZone::UTC));
}
TEST(AutoRespond, TimerDSTSkipsSpringGapAndUsesFirstAutumnOccurrence)
{
    const QTimeZone zone("Europe/London");
    ASSERT_TRUE(zone.isValid());
    AutoRespondTimer r;
    r.enabled = true;
    r.time = "01:30";
    const QDateTime spring(QDate(2026, 3, 29), QTime(0, 0), zone);
    const auto next = nextAutoRespondFire(r, spring, {}, zone);
    ASSERT_TRUE(next);
    EXPECT_EQ(next->date(), QDate(2026, 3, 30));
    const QDateTime autumn(QDate(2026, 10, 25), QTime(0, 0), zone);
    const auto fall = nextAutoRespondFire(r, autumn, {}, zone);
    ASSERT_TRUE(fall);
    EXPECT_EQ(fall->offsetFromUtc(), 3600);
    EXPECT_EQ(nextAutoRespondFire(r, fall->addSecs(1), {}, zone)->date(),
              QDate(2026, 10, 26));
}
TEST(AutoRespond, SettingsRoundTripAndMissingKeysKeepSafeDefaults)
{
    rapidjson::Document doc;
    doc.SetObject();
    auto r = trigger();
    r.channel = "kick:closed";
    r.regex = true;
    r.caseSensitive = true;
    r.sound = false;
    r.customSound = "file:///C:/sound.wav";
    r.cooldownSeconds = 45;
    r.delayMs = 300;
    const auto encoded =
        pajlada::Serialize<AutoRespondTrigger>::get(r, doc.GetAllocator());
    bool error = false;
    EXPECT_EQ(pajlada::Deserialize<AutoRespondTrigger>::get(encoded, &error),
              r);
    EXPECT_FALSE(error);
    AutoRespondTimer t;
    t.id = "timer";
    t.enabled = true;
    t.days = 65;
    t.channel = "twitch:closed";
    const auto timer =
        pajlada::Serialize<AutoRespondTimer>::get(t, doc.GetAllocator());
    EXPECT_EQ(pajlada::Deserialize<AutoRespondTimer>::get(timer, &error), t);
    const auto missing =
        pajlada::Deserialize<AutoRespondTrigger>::get(doc, &error);
    EXPECT_FALSE(missing.enabled);
    EXPECT_EQ(missing.cooldownSeconds, 30);
    EXPECT_EQ(missing.delayMs, 0);
    // Isolated paths: emulate settings.json without any auto respond keys.
    // Uses Setting's actual default path; no GUI or account/send mocks needed.
    chatterino::BoolSetting enabled("/tests/stage-e/missing/enabled", false);
    chatterino::IntSetting cap("/tests/stage-e/missing/perMinute", 6);
    EXPECT_FALSE(enabled.getValue());
    EXPECT_EQ(cap.getValue(), 6);
}
