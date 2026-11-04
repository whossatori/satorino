// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondData.hpp"
#include "controllers/autorespond/AutoRespondConnectionState.hpp"
#include "controllers/autorespond/AutoRespondController.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "controllers/autorespond/AutoRespondSend.hpp"
#include "common/Channel.hpp"

#include "Test.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <filesystem>

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
        expandAutoRespond(u"{user}|{channel}|{match}|{1}|{2}|{9}|{unknown}",
                          input, *match),
        "{channel}|foo|raffle 42|raffle|42||{unknown}");
}
TEST(AutoRespond, ResponsesAreOneLineAndBoundedWithoutSplittingUnicode)
{
    EXPECT_EQ(sanitizeAutoRespond(QString::fromUtf8("a\r\nb\t\x01\xe2\x80\xa8"
                                                    "c")),
              "a b c");
    EXPECT_EQ(sanitizeAutoRespond(QString(1000, u'x')).size(), 500);
    EXPECT_EQ(sanitizeAutoRespond(QString(1000, u'x'), 1000).size(), 500);
    EXPECT_TRUE(sanitizeAutoRespond(u"hello", 0).isEmpty());
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
    // The test harness loads settings from a fresh temporary directory. Read
    // the actual new keys, absent from that existing settings.json schema.
    AutoRespondSettings defaults;
    EXPECT_FALSE(defaults.enabled.getValue());
    EXPECT_EQ(defaults.perMinute.getValue(), 6);
    EXPECT_TRUE(defaults.showNote.getValue());
    EXPECT_TRUE(defaults.triggers.getValueCopy().empty());
    EXPECT_TRUE(defaults.timers.getValueCopy().empty());
}

TEST(AutoRespond, HistoryRepostsAndSpecialMessageFlagsAreNeverLive)
{
    using namespace chatterino;
    EXPECT_TRUE(isAutoRespondLiveMessage(MessageContext::Original, {}));
    EXPECT_FALSE(isAutoRespondLiveMessage(MessageContext::Repost, {}));
    for (const auto flag :
         {MessageFlag::RecentMessage, MessageFlag::System, MessageFlag::Whisper,
          MessageFlag::Disabled, MessageFlag::AutoMod,
          MessageFlag::AutoModOffendingMessage, MessageFlag::SharedMessage,
          MessageFlag::ModerationAction})
    {
        EXPECT_FALSE(
            isAutoRespondLiveMessage(MessageContext::Original, {flag}));
    }
}

TEST(AutoRespond, FinalExpansionGuardIsScopedToAutoSends)
{
    class ExpandingChannel : public chatterino::Channel
    {
    public:
        ExpandingChannel()
            : Channel("test", Type::Misc)
        {
        }
        void sendMessage(const QString &message) override
        {
            this->wire =
                limitAutoRespondSend(message + "\n" + QString(600, u'x'), this);
        }
        QString wire;
    } channel;
    channel.sendMessage("normal");
    EXPECT_GT(channel.wire.size(), 500);
    EXPECT_TRUE(channel.wire.contains(u'\n'));
    const auto result = sendAutoRespond(channel, "auto");
    EXPECT_LE(result.prepared.toUtf8().size(), 500);
    EXPECT_FALSE(result.prepared.contains(u'\n'));
    EXPECT_FALSE(isAutoRespondSend());
    channel.sendMessage("normal again");
    EXPECT_GT(channel.wire.size(), 500);
}

TEST(AutoRespond, DisconnectedOccurrenceIsConsumedRatherThanRetried)
{
    AutoRespondTimer r;
    r.enabled = true;
    r.time = "12:00";
    ASSERT_TRUE(autoRespondTimerDue(r, utc(2, 11, 59, 59), utc(2, 12, 0), {},
                                    QTimeZone::UTC));
    // The scheduler records this local date even if no connected target exists.
    const QDate consumed(2026, 11, 2);
    EXPECT_FALSE(autoRespondTimerDue(r, utc(2, 11, 59, 59), utc(2, 12, 0),
                                     consumed, QTimeZone::UTC));
    const auto next =
        nextAutoRespondFire(r, utc(2, 12, 0, 1), consumed, QTimeZone::UTC);
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, utc(3, 12, 0));
}

TEST(AutoRespond, ExistingSettingsJsonWithoutNewKeysLoadsAndRoundTrips)
{
    using Manager = pajlada::Settings::SettingManager;
    using Option = pajlada::Settings::SettingOption;
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto name = directory.filePath("settings.json");
    const auto path = std::filesystem::path(name.toStdU16String());
    QFile file(name);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    const QByteArray oldSettings =
        R"({"appearance":{"theme":{"name":"Dark"}}})";
    ASSERT_EQ(file.write(oldSettings), oldSettings.size());
    file.close();
    auto manager = std::make_shared<Manager>();
    manager->saveMethod = Manager::SaveMethod::SaveManually;
    ASSERT_EQ(manager->load(path), Manager::LoadError::NoError);
    pajlada::Settings::Setting<bool> enabled("/satorino/autorespond/enabled",
                                             false, Option::CompareBeforeSet,
                                             manager);
    pajlada::Settings::Setting<int> cap("/satorino/autorespond/perMinute", 6,
                                        Option::CompareBeforeSet, manager);
    pajlada::Settings::Setting<bool> note("/satorino/autorespond/showNote",
                                          true, Option::CompareBeforeSet,
                                          manager);
    pajlada::Settings::Setting<std::vector<AutoRespondTrigger>> triggers(
        "/satorino/autorespond/triggers", {}, Option::CompareBeforeSet,
        manager);
    pajlada::Settings::Setting<std::vector<AutoRespondTimer>> timers(
        "/satorino/autorespond/timers", {}, Option::CompareBeforeSet, manager);
    EXPECT_FALSE(enabled.getValue());
    EXPECT_EQ(cap.getValue(), 6);
    EXPECT_TRUE(note.getValue());
    EXPECT_TRUE(triggers.getValueCopy().empty());
    EXPECT_TRUE(timers.getValueCopy().empty());
    auto r = trigger();
    r.label = "raffle";
    r.channel = "twitch:closed";
    r.regex = true;
    r.caseSensitive = true;
    r.sound = true;
    r.customSound = "file:///C:/sound.wav";
    r.cooldownSeconds = 45;
    r.delayMs = 750;
    AutoRespondTimer t;
    t.id = "t";
    t.enabled = true;
    t.label = "evening";
    t.channel = "kick:closed";
    t.message = "hello";
    t.time = "19:45";
    t.days = 65;
    triggers.setValue({r});
    timers.setValue({t});
    ASSERT_EQ(manager->save(), Manager::SaveResult::Success);
    auto reloaded = std::make_shared<Manager>();
    reloaded->saveMethod = Manager::SaveMethod::SaveManually;
    ASSERT_EQ(reloaded->load(path), Manager::LoadError::NoError);
    pajlada::Settings::Setting<std::vector<AutoRespondTrigger>> savedTriggers(
        "/satorino/autorespond/triggers", {}, Option::CompareBeforeSet,
        reloaded);
    pajlada::Settings::Setting<std::vector<AutoRespondTimer>> savedTimers(
        "/satorino/autorespond/timers", {}, Option::CompareBeforeSet, reloaded);
    EXPECT_EQ(savedTriggers.getValueCopy(), std::vector<AutoRespondTrigger>{r});
    EXPECT_EQ(savedTimers.getValueCopy(), std::vector<AutoRespondTimer>{t});
    pajlada::Settings::Setting<QString> theme(
        "/appearance/theme/name", QString(), Option::CompareBeforeSet,
        reloaded);
    EXPECT_EQ(theme.getValue(), "Dark");
}

TEST(AutoRespond, InvalidAndDisabledTimerSchedulesHaveNoDeadline)
{
    AutoRespondTimer r;
    EXPECT_EQ(r.days, 127);
    EXPECT_FALSE(nextAutoRespondFire(r, utc(2, 11, 0), {}, QTimeZone::UTC));
    r.enabled = true;
    r.time = "25:15";
    EXPECT_FALSE(nextAutoRespondFire(r, utc(2, 11, 0), {}, QTimeZone::UTC));
    r.time = "12:00";
    // A backward clock change must not reopen any already-consumed date.
    const auto next = nextAutoRespondFire(r, utc(2, 11, 0), QDate(2026, 11, 4),
                                          QTimeZone::UTC);
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, utc(5, 12, 0));
}

TEST(AutoRespond, QueuedUnconfirmedAndRejoinedRoomsCannotReceiveTimers)
{
    AutoRespondConnectionState state;
    const QString topic = "chatrooms.123.v2";
    EXPECT_FALSE(state.connected(topic, true, true));
    state.confirm(topic);
    EXPECT_TRUE(state.connected(topic, true, true));
    EXPECT_FALSE(state.connected(topic, false, true));
    EXPECT_FALSE(state.connected(topic, true, false));
    state.forget(topic);
    EXPECT_FALSE(state.connected(topic, true, true));
    state.confirm(topic);
    state.reset();
    EXPECT_FALSE(state.connected(topic, true, true));
}

TEST(AutoRespond, SendContextIsOwnedTargetScopedAndRejectsReentry)
{
    class ReentrantChannel : public chatterino::Channel
    {
    public:
        ReentrantChannel()
            : Channel("test", Type::Misc)
        {
        }
        void sendMessage(const QString &message) override
        {
            this->nested = sendAutoRespond(*this, "nested");
            this->prepared = limitAutoRespondSend(message + "\nnext", this);
            recordAutoRespondSend(this, true);
        }
        AutoRespondSubmission nested;
        QString prepared;
    } channel;
    const auto result = sendAutoRespond(channel, "outer");
    EXPECT_TRUE(channel.nested.prepared.isEmpty());
    EXPECT_EQ(result.prepared, "outer next");
    EXPECT_EQ(result.accepted, true);
    EXPECT_FALSE(isAutoRespondSend());
    recordAutoRespondSend(&channel, false);
    EXPECT_EQ(result.accepted,
              true);  // state cannot be written after the scope
}
TEST(AutoRespond, InactiveHookDoesNotCreateSettingsOrRequireAnApplication)
{
    EXPECT_EQ(tryAutoRespond(), nullptr);
    EXPECT_EQ(tryAutoRespondSettings(), nullptr);
    chatterino::Channel channel("not-connected",
                                chatterino::Channel::Type::Twitch);
    observeAutoRespond(channel, {}, chatterino::MessageContext::Original, {});
    EXPECT_EQ(tryAutoRespondSettings(), nullptr);
}
