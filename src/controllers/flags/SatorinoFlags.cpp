// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/flags/SatorinoFlags.hpp"
#include "Application.hpp"
#include "common/Channel.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "util/MultiChannel.hpp"
#include <QDateTime>
#include <memory>

namespace chatterino::satorino {
const FlagStore &localFlags()
{
    assertInGuiThread();
    static QString loaded;
    static FlagStore store;
    const auto json = getSettings()->satorinoFlagsState.getValue();
    if (json != loaded)
    {
        store = FlagStore::fromJson(json);
        loaded = json;
    }
    return store;
}
QString localFlagRoom(const Message &message, Channel *context)
{
    if (message.platform != MessagePlatform::AnyOrTwitch ||
        !validFlagId(message.userID))
        return {};
    const auto match = [&message](Channel *channel) -> QString {
        auto *twitch = dynamic_cast<TwitchChannel *>(channel);
        return twitch && channel->getType() == Channel::Type::Twitch
                   ? flagRoomKey(message.channelName, channel->getName(),
                                 twitch->roomId())
                   : QString{};
    };
    if (auto *multi = dynamic_cast<MultiChannel *>(context))
        for (const auto &child : multi->channels())
            if (const auto id = match(child.channel.get()); !id.isEmpty())
                return id;
    if (const auto id = match(context); !id.isEmpty())
        return id;
    // Read-only local registry lookup: does not create a channel, join, or request metadata.
    const auto channel =
        getApp()->getTwitch()->getChannelOrEmpty(message.channelName);
    return match(channel.get());
}
FlagStyle localFlagStyle(const Message &message, const QString &room,
                         bool ignore)
{
    if (!getSettings()->satorinoFlagsEnabled.getValue() ||
        message.flags.has(MessageFlag::System))
        return {};
    const bool higher =
        message.flags.has(MessageFlag::Highlighted) || message.reward ||
        message.flags.has(MessageFlag::RedeemedHighlight) ||
        message.flags.has(MessageFlag::RedeemedChannelPointReward) ||
        message.flags.has(MessageFlag::Subscription) ||
        message.flags.has(MessageFlag::ChatWarning) ||
        message.flags.has(MessageFlag::AutoMod) ||
        message.flags.has(MessageFlag::Debug);
    auto colour = QColor(getSettings()->satorinoFlagsColour.getValue());
    if (!colour.isValid())
        colour = QColor(FLAGS_COLOUR_DEFAULT);
    return flagStyle(true, localFlags().contains(room, message.userID),
                     getSettings()->satorinoFlagsMark.getValue(), higher,
                     ignore, colour);
}
QString toggleLocalFlag(const QString &room, const QString &user,
                        const QString &channelLabel, const QString &userLabel)
{
    assertInGuiThread();
    if (!getSettings()->satorinoFlagsEnabled.getValue())
        return "Local flags are off";
    auto store = localFlags();
    QString error;
    if (!store.toggle({room, user, channelLabel, userLabel,
                       QDateTime::currentMSecsSinceEpoch()},
                      &error))
        return error;
    getSettings()->satorinoFlagsState.setValue(store.toJson());
    return {};
}
}  // namespace chatterino::satorino
