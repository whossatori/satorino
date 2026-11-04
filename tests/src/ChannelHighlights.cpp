// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/highlights/ChannelHighlightData.hpp"

#include "messages/Message.hpp"
#include "messages/MessageParseArgs.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "Test.hpp"

using namespace chatterino;
using namespace chatterino::satorino;

namespace {

struct PhraseContext {
    Message message;
    MessageParseArgs args;
    std::vector<TwitchBadge> badges;
    QString sender = "other";
    QString text;

    HighlightCheckParams params(bool self = false) const
    {
        return {
            .args = this->args,
            .twitchBadges = this->badges,
            .senderName = this->sender,
            .originalMessage = this->text,
            .messageFlags = this->message.flags,
            .self = self,
            .runContext = {.message = this->message, .channel = nullptr},
        };
    }
};

}  // namespace

TEST(ChannelHighlights, KeysNormalizePlatformChannelAndOptionalHashPrefix)
{
    EXPECT_EQ(channelHighlightKey({u" Twitch ", u" #FoO "}), "twitch:foo");
    EXPECT_EQ(channelHighlightKey({u"YouTube", u"ABC_123"}), "youtube:abc_123");
    EXPECT_EQ(channelHighlightKey({u"kick", u"Älice"}),
              QString::fromUtf8("kick:älice"));
    EXPECT_TRUE(channelHighlightKey({u"twitch", u"#"}).isEmpty());
    EXPECT_TRUE(channelHighlightKey({u"", u"foo"}).isEmpty());
    const ChannelHighlightKeyHash hash;
    const ChannelHighlightKeyEqual equal;
    const auto stored = QStringLiteral("twitch:foo");
    const ChannelHighlightKeyView borrowed{u"TWITCH", u"#FoO"};
    EXPECT_EQ(hash(stored), hash(borrowed));
    EXPECT_TRUE(equal(stored, borrowed));
    EXPECT_TRUE(equal(borrowed, stored));
}

TEST(ChannelHighlights, LookupIsSharedByCaseAndIsolatedByPlatformAndChannel)
{
    auto phrase = ChannelHighlightPhrase::createDefault();
    ChannelHighlightIndex index;
    index.rebuild({{"twitch:foo", {phrase}},
                   {"kick:foo", {phrase}},
                   {"youtube:empty", {}}});
    const auto *first = index.find({u"twitch", u"foo"});
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, index.find({u"TWITCH", u"#FOO"}));
    ASSERT_NE(index.find({u"kick", u"foo"}), nullptr);
    EXPECT_NE(first, index.find({u"kick", u"foo"}));
    EXPECT_EQ(index.find({u"twitch", u"bar"}), nullptr);
    EXPECT_EQ(index.find({u"youtube", u"foo"}), nullptr);
    EXPECT_EQ(index.find({u"youtube", u"empty"}), nullptr);
}

TEST(ChannelHighlights, InvalidAndEmptyPatternsHaveNoCompiledEntry)
{
    auto phrase = ChannelHighlightPhrase::createDefault();
    phrase.isRegex = true;
    phrase.pattern = "[";
    ChannelHighlightIndex index;
    index.rebuild({{"twitch:invalid", {phrase}}, {"twitch:empty", {}}});
    EXPECT_EQ(index.find({u"twitch", u"invalid"}), nullptr);
    phrase.pattern.clear();
    index.rebuild({{"twitch:foo", {phrase}}});
    EXPECT_EQ(index.find({u"twitch", u"foo"}), nullptr);
}

TEST(ChannelHighlights, ChannelColorAndSoundOverrideEvenAFullGlobalResult)
{
    HighlightEvaluation global{true,
                               {.alert = true,
                                .sound = QUrl("qrc:/global.wav"),
                                .color = std::make_shared<QColor>(Qt::red),
                                .showInMentions = true}};
    ASSERT_TRUE(global.second.full());
    HighlightEvaluation local{true,
                              {.alert = false,
                               .sound = QUrl("qrc:/channel.wav"),
                               .color = std::make_shared<QColor>(Qt::blue),
                               .showInMentions = false}};
    const auto merged = applyChannelHighlightPriority(global, local);
    EXPECT_TRUE(merged.first);
    ASSERT_NE(merged.second.color, nullptr);
    EXPECT_EQ(*merged.second.color, QColor(Qt::blue));
    EXPECT_EQ(merged.second.sound, local.second.sound);
    EXPECT_TRUE(merged.second.alert);
    EXPECT_TRUE(merged.second.showInMentions);
}

TEST(ChannelHighlights, ExplicitChannelSilenceWinsAndLocalFlagsAreCombined)
{
    HighlightEvaluation global{true,
                               {.alert = false,
                                .sound = QUrl("qrc:/global.wav"),
                                .color = std::make_shared<QColor>(Qt::red),
                                .showInMentions = false}};
    HighlightEvaluation local{
        true,
        {.alert = true, .sound = {}, .color = nullptr, .showInMentions = true}};
    const auto merged = applyChannelHighlightPriority(global, local);
    EXPECT_TRUE(merged.second.sound.isEmpty());
    EXPECT_EQ(merged.second.color, nullptr);
    EXPECT_TRUE(merged.second.alert);
    EXPECT_TRUE(merged.second.showInMentions);
    const auto unchanged = applyChannelHighlightPriority(
        global, {false, HighlightResult::emptyResult()});
    EXPECT_EQ(unchanged.second.sound, global.second.sound);
    EXPECT_EQ(unchanged.second.color, global.second.color);
}

TEST(ChannelHighlights, UsesGlobalLiteralBoundariesAndSelfSuppression)
{
    auto phrase = ChannelHighlightPhrase::createDefault();
    phrase.pattern = "cat";
    ChannelHighlightIndex index;
    index.rebuild({{"twitch:foo", {phrase}}});
    const auto *checks = index.find({u"twitch", u"foo"});
    ASSERT_NE(checks, nullptr);
    PhraseContext context;
    context.text = "concatenate";
    EXPECT_FALSE(evaluateChannelPhrases(*checks, context.params()).first);
    context.text = "the CAT!";
    EXPECT_TRUE(evaluateChannelPhrases(*checks, context.params()).first);
    EXPECT_FALSE(evaluateChannelPhrases(*checks, context.params(true)).first);
}

TEST(ChannelHighlights, UsesGlobalRegexCaseAndDefaultOrCustomSounds)
{
    auto phrase = ChannelHighlightPhrase::createDefault();
    phrase.pattern = "Fo+";
    phrase.isRegex = true;
    phrase.isCaseSensitive = true;
    phrase.hasSound = true;
    ChannelHighlightIndex index;
    index.rebuild({{"twitch:foo", {phrase}}});
    PhraseContext context;
    context.text = "foo";
    ASSERT_NE(index.find({u"twitch", u"foo"}), nullptr);
    EXPECT_FALSE(evaluateChannelPhrases(*index.find({u"twitch", u"foo"}),
                                        context.params())
                     .first);
    context.text = "Fooo";
    const auto result = evaluateChannelPhrases(*index.find({u"twitch", u"foo"}),
                                               context.params());
    EXPECT_TRUE(result.first);
    EXPECT_EQ(result.second.sound, QUrl("qrc:/sounds/ping2.wav"));
    phrase.soundUrl = QUrl::fromLocalFile("C:/sounds/custom.wav");
    index.rebuild({{"twitch:foo", {phrase}}});
    EXPECT_EQ(evaluateChannelPhrases(*index.find({u"twitch", u"foo"}),
                                     context.params())
                  .second.sound,
              phrase.soundUrl);
}

TEST(ChannelHighlights, SettingsCodecPreservesEveryPhraseOption)
{
    auto phrase = ChannelHighlightPhrase::createDefault();
    phrase.pattern = "hello.*world";
    phrase.isRegex = true;
    phrase.isCaseSensitive = true;
    phrase.hasSound = false;
    phrase.soundUrl = QUrl::fromLocalFile("C:/sounds/saved-while-disabled.wav");
    phrase.color = QColor(20, 40, 60, 90);
    const ChannelHighlightRules rules{{"youtube:abc", {phrase}}};
    rapidjson::Document document;
    const auto encoded = pajlada::Serialize<ChannelHighlightRules>::get(
        rules, document.GetAllocator());
    bool error = false;
    const auto decoded =
        pajlada::Deserialize<ChannelHighlightRules>::get(encoded, &error);
    EXPECT_FALSE(error);
    EXPECT_EQ(rules, decoded);
}
