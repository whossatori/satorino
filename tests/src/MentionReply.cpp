// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/MentionReply.hpp"

#include "messages/MessageThread.hpp"
#include "Test.hpp"

using namespace chatterino;

namespace {

const auto NOW = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);

MessagePtrMut mention()
{
    auto message = std::make_shared<Message>();
    message->channelName = "#SomeChannel";
    message->id = "parent-id";
    message->loginName = "somebody";
    message->displayName = "Somebody";
    message->messageText = "@satori hello";
    message->serverReceivedTime = NOW.addSecs(-10);
    return message;
}

}  // namespace

TEST(MentionReply, ExtractsOriginalChannelAndMessage)
{
    const auto message = mention();
    const auto result = mentionReplyTarget(*message, NOW);
    ASSERT_TRUE(result.target);
    EXPECT_TRUE(result.unavailableReason.isEmpty());
    EXPECT_EQ(result.target->channelName, "somechannel");
    EXPECT_EQ(result.target->parentMessageId, "parent-id");
    EXPECT_EQ(result.target->parentLogin, "somebody");
    EXPECT_EQ(result.target->parentDisplayName, "Somebody");
    EXPECT_EQ(result.target->parentDisplayText, "@satori hello");
    EXPECT_EQ(result.target->platform, MessagePlatform::AnyOrTwitch);
}

TEST(MentionReply, MissingMessageIdIsUnavailable)
{
    auto message = mention();
    message->id.clear();
    auto result = mentionReplyTarget(*message, NOW);
    EXPECT_FALSE(result.target);
    EXPECT_FALSE(result.unavailableReason.isEmpty());
}

TEST(MentionReply, DeletedMessageIsUnavailable)
{
    for (auto flag : {MessageFlag::InvalidReplyTarget, MessageFlag::Disabled})
    {
        auto message = mention();
        message->flags.set(flag);
        auto result = mentionReplyTarget(*message, NOW);
        EXPECT_FALSE(result.target);
        EXPECT_FALSE(result.unavailableReason.isEmpty());
    }
}

TEST(MentionReply, LeftOriginHasExplicitReason)
{
    auto message = mention();
    auto result = mentionReplyTarget(*message, NOW, false);
    EXPECT_FALSE(result.target);
    EXPECT_EQ(result.unavailableReason,
              "can't reply: you are not connected to #somechannel");
}

TEST(MentionReply, ReplyTargetsThreadRoot)
{
    auto root = mention();
    auto reply = mention();
    reply->id = "child-id";
    reply->loginName = "child";
    reply->displayName = "Child";
    reply->messageText = "child text";
    reply->replyThread = std::make_shared<MessageThread>(root);
    auto result = mentionReplyTarget(*reply, NOW);
    ASSERT_TRUE(result.target);
    EXPECT_EQ(result.target->parentMessageId, root->id);
    EXPECT_EQ(result.target->parentLogin, root->loginName);
    EXPECT_EQ(result.target->parentDisplayName, root->displayName);
    EXPECT_EQ(result.target->parentDisplayText, root->messageText);
}

TEST(MentionReply, UnavailableThreadRootIsRejected)
{
    auto root = mention();
    auto reply = mention();
    reply->replyThread = std::make_shared<MessageThread>(root);
    root->flags.set(MessageFlag::InvalidReplyTarget);
    EXPECT_FALSE(mentionReplyTarget(*reply, NOW).target);
    root->flags.unset(MessageFlag::InvalidReplyTarget);
    root->id.clear();
    EXPECT_FALSE(mentionReplyTarget(*reply, NOW).target);
}

TEST(MentionReply, CrossChannelOrPlatformRootIsRejected)
{
    auto root = mention();
    auto reply = mention();
    reply->replyThread = std::make_shared<MessageThread>(root);
    root->channelName = "differentchannel";
    EXPECT_FALSE(mentionReplyTarget(*reply, NOW).target);
    root->channelName = reply->channelName;
    root->platform = MessagePlatform::Kick;
    EXPECT_FALSE(mentionReplyTarget(*reply, NOW).target);
}

TEST(MentionReply, OwnMessageIsReplyable)
{
    auto message = mention();
    message->loginName = "satori";
    message->displayName = "Satori";
    auto result = mentionReplyTarget(*message, NOW);
    ASSERT_TRUE(result.target);
    EXPECT_EQ(result.target->parentLogin, "satori");
    EXPECT_EQ(result.target->parentMessageId, message->id);
}

TEST(MentionReply, KickKeepsPlatformAndOrigin)
{
    auto message = mention();
    message->platform = MessagePlatform::Kick;
    message->channelName = ":kick:#Some_Channel";
    auto result = mentionReplyTarget(*message, NOW);
    ASSERT_TRUE(result.target);
    EXPECT_EQ(result.target->platform, MessagePlatform::Kick);
    EXPECT_EQ(result.target->channelName, "some_channel");
    EXPECT_EQ(result.target->parentMessageId, message->id);
}

TEST(MentionReply, UnsupportedPlatformIsUnavailable)
{
    auto message = mention();
    message->platform = MessagePlatform::YouTube;
    auto result = mentionReplyTarget(*message, NOW);
    EXPECT_FALSE(result.target);
    EXPECT_FALSE(result.unavailableReason.isEmpty());
}

TEST(MentionReply, MissingOrPseudochannelOriginIsUnavailable)
{
    auto message = mention();
    for (const auto &name : {"", "/mentions", "bad channel"})
    {
        message->channelName = name;
        auto result = mentionReplyTarget(*message, NOW);
        EXPECT_FALSE(result.target);
        EXPECT_FALSE(result.unavailableReason.isEmpty());
    }
}

TEST(MentionReply, PreservesStockAgeAndSystemMessageRestrictions)
{
    auto message = mention();
    message->serverReceivedTime = NOW.addDays(-1).addSecs(-1);
    EXPECT_FALSE(mentionReplyTarget(*message, NOW).target);
    message->serverReceivedTime = NOW;
    message->flags.set(MessageFlag::System);
    EXPECT_FALSE(mentionReplyTarget(*message, NOW).target);
    message->flags.unset(MessageFlag::System);
    message->loginName.clear();
    EXPECT_FALSE(mentionReplyTarget(*message, NOW).target);
}

TEST(MentionReply, ReplyWithoutLoadedRootIsUnavailable)
{
    auto message = mention();
    message->flags.set(MessageFlag::ReplyMessage);
    auto result = mentionReplyTarget(*message, NOW);
    EXPECT_FALSE(result.target);
    EXPECT_EQ(result.unavailableReason,
              "can't reply: the original thread is unavailable");
}
