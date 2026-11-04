// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/MentionReply.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "messages/MessageThread.hpp"
#include "providers/kick/KickApi.hpp"
#include "providers/kick/KickChannel.hpp"
#include "providers/kick/KickChatServer.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"

#include <algorithm>
#include <utility>

namespace chatterino {
namespace {

QString originName(const Message &message)
{
    auto name = message.channelName;
    if (message.platform == MessagePlatform::Kick && name.startsWith(":kick:"))
    {
        name.remove(0, 6);
    }
    if (name.startsWith('#'))
    {
        name.remove(0, 1);
    }
    return name.toLower();
}

bool replyable(const Message &message, const QDateTime &now)
{
    return !message.id.isEmpty() && !message.loginName.isEmpty() &&
           !message.flags.hasAny(
               {MessageFlag::System, MessageFlag::Subscription,
                MessageFlag::Timeout, MessageFlag::Whisper,
                MessageFlag::ModerationAction, MessageFlag::InvalidReplyTarget,
                MessageFlag::Disabled}) &&
           message.serverReceivedTime.secsTo(now) <= 24 * 60 * 60;
}

}  // namespace

MentionReplyResult mentionReplyTarget(const Message &message,
                                      const QDateTime &now,
                                      bool originAvailable)
{
    if (message.platform == MessagePlatform::YouTube)
    {
        return {{}, "can't reply: this platform does not support replies"};
    }
    if (!replyable(message, now))
    {
        return {{}, "can't reply: this message is unavailable for replies"};
    }

    const auto name = originName(message);
    if (name.isEmpty() || !std::all_of(name.begin(), name.end(), [](QChar ch) {
            return ch.isLetterOrNumber() || ch == '_' || ch == '-';
        }))
    {
        return {{}, "can't reply: the origin channel is unavailable"};
    }
    if (!originAvailable)
    {
        return {{}, "can't reply: you are not connected to #" + name};
    }

    if (message.flags.has(MessageFlag::ReplyMessage) && !message.replyThread)
    {
        return {{}, "can't reply: the original thread is unavailable"};
    }

    const auto *parent = &message;
    if (message.replyThread)
    {
        parent = message.replyThread->root().get();
        if (!parent || !replyable(*parent, now) ||
            parent->platform != message.platform || originName(*parent) != name)
        {
            return {{}, "can't reply: the original thread is unavailable"};
        }
    }

    return {MentionReplyTarget{name, parent->id, parent->loginName,
                               parent->displayName, parent->messageText,
                               message.platform},
            {}};
}

ChannelPtr findMentionReplyChannel(const MentionReplyTarget &target)
{
    assertInGuiThread();
    auto *app = getApp();
    ChannelPtr channel;
    if (target.platform == MessagePlatform::AnyOrTwitch)
    {
        if (!app->getAccounts()->twitch.isLoggedIn())
        {
            return {};
        }
        channel = app->getTwitch()->getChannelOrEmpty(target.channelName);
        auto twitchChannel = std::dynamic_pointer_cast<TwitchChannel>(channel);
        if (!twitchChannel || twitchChannel->roomId().isEmpty() ||
            twitchChannel->isDisconnected())
        {
            return {};
        }
    }
    else if (target.platform == MessagePlatform::Kick)
    {
        auto *server = app->getKickChatServer();
        if (!server)
        {
            return {};
        }
        auto kickChannel =
            server->findBySlug(KickApi::slugify(target.channelName));
        if (!kickChannel || kickChannel->userID() == 0)
        {
            return {};
        }
        channel = std::move(kickChannel);
    }
    if (!channel || !channel->isWritable() || !channel->canSendMessage())
    {
        return {};
    }
    return channel;
}

}  // namespace chatterino
