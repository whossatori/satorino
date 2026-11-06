// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/flags/SatorinoSuspiciousUsers.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"
#include <QColor>

namespace chatterino::satorino {
SuspiciousUserCache &nativeSuspiciousUsers()
{
    assertInGuiThread();
    static SuspiciousUserCache cache;
    return cache;
}
pajlada::Signals::Signal<QString, QString> &nativeSuspiciousUsersChanged()
{
    assertInGuiThread();
    static pajlada::Signals::Signal<QString, QString> changed;
    return changed;
}
void notifyNativeSuspiciousUser(const QString &room, const QString &user)
{
    nativeSuspiciousUsersChanged().invoke(room, user);
}
void observeNativeSuspiciousUser(const QString &room, const QString &user,
                                 SuspiciousState state, qint64 atMs)
{
    if (nativeSuspiciousUsers().observe(room, user, state, atMs))
        notifyNativeSuspiciousUser(room, user);
}
FlagStyle nativeFlagStyleForMessage(const Message &message, const QString &room,
                                    bool ignoreHighlights)
{
    if (!getSettings()->satorinoNativeFlagsEnabled.getValue())
        return {};
    const bool ordinary = message.platform == MessagePlatform::AnyOrTwitch &&
                          flagTarget(room, message.userID).has_value() &&
                          !message.flags.has(MessageFlag::System) &&
                          !message.flags.has(MessageFlag::LowTrustUsers) &&
                          !message.flags.has(MessageFlag::ModerationAction) &&
                          !message.flags.has(MessageFlag::Subscription) &&
                          !message.flags.has(MessageFlag::AutoMod) &&
                          !message.flags.has(MessageFlag::ChatWarning) &&
                          !message.flags.has(MessageFlag::Debug);
    if (!ordinary)
        return {};
    const auto state =
        nativeFlagState(nativeSuspiciousUsers().state(room, message.userID),
                        message.flags.has(MessageFlag::MonitoredMessage),
                        message.flags.has(MessageFlag::RestrictedMessage));
    if (state != SuspiciousState::Monitored &&
        state != SuspiciousState::Restricted)
        return {};
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    // Routed mentions and highlighted username mentions keep their existing priority,
    // including when the username rule's Show in mentions option is disabled.
    const bool mention =
        message.flags.has(MessageFlag::Highlighted) &&
        (message.flags.has(MessageFlag::ShowInMentions) ||
         (account &&
          nativeFlagMentions(message.messageText, account->getUserName())));
    const bool protectedHighlight =
        mention || message.reward ||
        message.flags.has(MessageFlag::RedeemedHighlight) ||
        message.flags.has(MessageFlag::RedeemedChannelPointReward) ||
        message.flags.has(MessageFlag::Subscription) ||
        message.flags.has(MessageFlag::ChatWarning) ||
        message.flags.has(MessageFlag::AutoMod) ||
        message.flags.has(MessageFlag::Debug);
    return nativeFlagStyle(
        state, true, ordinary, protectedHighlight, ignoreHighlights,
        QColor(getSettings()->satorinoNativeMonitoredColour.getValue()),
        QColor(getSettings()->satorinoNativeRestrictedColour.getValue()));
}
}  // namespace chatterino::satorino
