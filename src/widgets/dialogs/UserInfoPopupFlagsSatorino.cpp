// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/UserInfoPopup.hpp"
#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/flags/SatorinoFlags.hpp"
#include "controllers/flags/SatorinoSuspiciousUsers.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "util/PostToThread.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include <QAction>
#include <QCursor>
#include <QDateTime>
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QToolTip>
#include <algorithm>
#include <memory>
#include <optional>

namespace chatterino {
QString UserInfoPopup::flagTargetId() const
{
    if (satorino::validFlagId(this->userId_))
        return this->userId_;
    if (!this->underlyingChannel_ || this->userName_.isEmpty())
        return {};
    // Already-buffered authoritative author IDs also work offline, before the ordinary profile lookup.
    const auto messages = this->underlyingChannel_->getMessageSnapshot();
    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
        if ((*it)->platform == MessagePlatform::AnyOrTwitch &&
            (*it)->loginName.compare(this->userName_, Qt::CaseInsensitive) ==
                0 &&
            satorino::validFlagId((*it)->userID))
            return (*it)->userID;
    return {};
}
QString UserInfoPopup::flagRoomId() const
{
    // satorino: the popup's message context, never the currently selected split.
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    return channel && channel->getType() == Channel::Type::Twitch &&
                   !this->isKick_ && !this->isYouTube_
               ? channel->roomId()
               : QString{};
}
bool UserInfoPopup::nativeFlagVisible() const
{
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    const bool real = channel && channel->getType() == Channel::Type::Twitch &&
                      !this->isKick_ && !this->isYouTube_;
    auto role = satorino::FlagTargetRole::Unknown;
    if (account && (account->getUserId() == this->flagTargetId() ||
                    account->getUserName().compare(this->userName_,
                                                   Qt::CaseInsensitive) == 0))
        role = satorino::FlagTargetRole::Self;
    else if (this->isBroadcaster_ ||
             (channel && this->userName_.compare(channel->getName(),
                                                 Qt::CaseInsensitive) == 0))
        role = satorino::FlagTargetRole::Broadcaster;
    else if (this->isMod_)
        role = satorino::FlagTargetRole::Moderator;
    else if (this->nativeFlagRolesKnown_)
        role = satorino::FlagTargetRole::Regular;
    return satorino::flagControlVisible(
        real,
        account && !account->isAnon() &&
            satorino::validFlagId(account->getUserId()) && channel &&
            channel->hasModRights() && this->shouldShowModerationActions(),
        role);
}
void UserInfoPopup::bindNativeFlagButton()
{
    QObject::connect(this->ui_.localFlagModeration, &Button::leftClicked, this,
                     &UserInfoPopup::showNativeFlagMenu);
    this->signalHolder_.managedConnect(
        satorino::nativeSuspiciousUsersChanged(),
        [this](const QString &room, const QString &user) {
            if (room == this->flagRoomId() && user == this->flagTargetId())
                this->refreshNativeFlagButton();
        });
    this->refreshNativeFlagButton();
}
void UserInfoPopup::seedNativeFlagState()
{
    const auto room = this->flagRoomId();
    const auto user = this->flagTargetId();
    auto &cache = satorino::nativeSuspiciousUsers();
    if (!satorino::flagTarget(room, user) || !this->underlyingChannel_ ||
        cache.state(room, user) != satorino::SuspiciousState::Unknown ||
        cache.busy(room, user))
        return;
    // Only positive explicit flags are evidence. Absence of a flag never means None.
    const auto messages = this->underlyingChannel_->getMessageSnapshot();
    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
        const auto &message = **it;
        if (message.platform != MessagePlatform::AnyOrTwitch ||
            message.channelName.compare(this->underlyingChannel_->getName(),
                                        Qt::CaseInsensitive) != 0 ||
            message.loginName.compare(this->userName_, Qt::CaseInsensitive) !=
                0 ||
            (!message.userID.isEmpty() && message.userID != user))
            continue;
        const auto state = message.flags.has(MessageFlag::RestrictedMessage)
                               ? satorino::SuspiciousState::Restricted
                           : message.flags.has(MessageFlag::MonitoredMessage)
                               ? satorino::SuspiciousState::Monitored
                               : satorino::SuspiciousState::Unknown;
        if (state != satorino::SuspiciousState::Unknown)
        {
            satorino::observeNativeSuspiciousUser(
                room, user, state,
                message.serverReceivedTime.toMSecsSinceEpoch());
            break;
        }
    }
}
void UserInfoPopup::refreshNativeFlagButton()
{
    if (!this->ui_.localFlagModeration)
        return;
    auto *button = this->ui_.localFlagModeration;
    button->setVisible(this->nativeFlagVisible());
    this->seedNativeFlagState();
    const auto room = this->flagRoomId();
    const auto user = this->flagTargetId();
    const auto &cache = satorino::nativeSuspiciousUsers();
    const bool ready = satorino::flagTarget(room, user).has_value();
    button->setText(satorino::suspiciousControlLabel(cache.state(room, user)));
    button->setEnabled(ready && !cache.busy(room, user));
    button->setToolTip(
        !ready ? "Waiting for channel/user ID"
        : cache.busy(room, user)
            ? "Updating suspicious user status..."
            : "Twitch suspicious user controls (moderators only)");
}
void UserInfoPopup::showFlagError(const QString &error)
{
    this->ui_.localFlagModeration->setToolTip(error);
    QToolTip::showText(QCursor::pos(), error, this);
}
void UserInfoPopup::showNativeFlagMenu()
{
    this->refreshNativeFlagButton();
    const auto room = this->flagRoomId();
    const auto user = this->flagTargetId();
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!this->nativeFlagVisible() || !satorino::flagTarget(room, user) ||
        satorino::nativeSuspiciousUsers().busy(room, user) || !account)
        return;
    const auto moderator = account->getUserId();
    const auto state = satorino::nativeSuspiciousUsers().state(room, user);
    auto *button = this->ui_.localFlagModeration;
    // Same app-themed QMenu as the usercard's role/name menus; inherit scaled font.
    auto *menu = new QMenu(this);
    menu->setFont(button->font());
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const bool pinned = this->ensurePinned();
    QObject::connect(menu, &QMenu::aboutToHide, this, [this, pinned] {
        if (pinned)
            this->togglePinned();
    });
    for (const auto action : satorino::suspiciousMenuActions(state))
        menu->addAction(
            satorino::suspiciousActionLabel(action, state, this->userName_),
            this, [this, action, room, user, moderator] {
                // satorino: the only path that may mutate Twitch is an explicit menu selection.
                this->runNativeFlagAction(action, room, user, moderator);
            });

    // satorino: isolated local-highlight extra. Delete this block and toggleLocalHighlight to remove it.
    menu->addSeparator();
    const bool highlighted = satorino::localFlags().contains(room, user);
    auto *highlight = menu->addAction(
        highlighted ? "Remove highlight" : "Highlight in my chat", this,
        [this, room, user, moderator] {
            const auto current = getApp()->getAccounts()->twitch.getCurrent();
            if (this->nativeFlagVisible() && this->flagRoomId() == room &&
                this->flagTargetId() == user && current &&
                current->getUserId() == moderator)
                this->toggleLocalHighlight();
        });
    const auto local = satorino::flagControlState(
        satorino::localFlags(), room, user,
        getSettings()->satorinoFlagsEnabled.getValue());
    highlight->setEnabled(local.enabled);
    highlight->setToolTip(local.tooltip);
    menu->setToolTipsVisible(true);
    menu->popup(button->mapToGlobal(QPoint(0, button->height())));
    menu->raise();
}
void UserInfoPopup::runNativeFlagAction(satorino::SuspiciousAction action,
                                        const QString &room,
                                        const QString &user,
                                        const QString &moderator)
{
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!this->nativeFlagVisible() || !account || account->isAnon() ||
        account->getUserId() != moderator || room != this->flagRoomId() ||
        user != this->flagTargetId() || !satorino::flagTarget(room, user))
    {
        this->showFlagError(
            "Channel, account or target role changed; reopen the flag menu.");
        return;
    }
    auto &cache = satorino::nativeSuspiciousUsers();
    const auto actions =
        satorino::suspiciousMenuActions(cache.state(room, user));
    if (std::find(actions.begin(), actions.end(), action) == actions.end())
    {
        this->showFlagError(
            "Suspicious user status changed; reopen the flag menu.");
        return;
    }
    const auto token = cache.begin(room, user);
    if (!token)
    {
        this->showFlagError("A suspicious user request is already pending or "
                            "the request limit was reached.");
        return;
    }
    satorino::notifyNativeSuspiciousUser(room, user);
    const auto outcome = action == satorino::SuspiciousAction::Monitor
                             ? satorino::SuspiciousState::Monitored
                         : action == satorino::SuspiciousAction::Restrict
                             ? satorino::SuspiciousState::Restricted
                             : satorino::SuspiciousState::None;
    QPointer<UserInfoPopup> self(this);
    const auto success = [room, user, token = *token, outcome] {
        runInGuiThread([room, user, token, outcome] {
            if (!satorino::nativeSuspiciousUsers().complete(
                    room, user, token, outcome,
                    QDateTime::currentMSecsSinceEpoch()))
                return;
            satorino::notifyNativeSuspiciousUser(room, user);
        });
    };
    const auto failure = [self, room, user, moderator,
                          token = *token](const QString &error) {
        runInGuiThread([self, room, user, moderator, token, error] {
            if (!satorino::nativeSuspiciousUsers().complete(
                    room, user, token, std::nullopt,
                    QDateTime::currentMSecsSinceEpoch()))
                return;
            satorino::notifyNativeSuspiciousUser(room, user);
            const auto current = getApp()->getAccounts()->twitch.getCurrent();
            if (self && current && current->getUserId() == moderator &&
                self->flagRoomId() == room && self->flagTargetId() == user)
                self->showFlagError(
                    "Failed to update suspicious user - " + error +
                    "\nIf your login predates "
                    "moderator:manage:suspicious_users, log in again.");
        });
    };
    // Reuse precisely the Helix path used by /monitor, /restrict, /unmonitor, /unrestrict.
    if (action == satorino::SuspiciousAction::Remove)
        getHelix()->removeSuspiciousUser(room, moderator, user, success,
                                         failure);
    else
        getHelix()->addSuspiciousUser(
            room, moderator, user,
            action == satorino::SuspiciousAction::Restrict, success, failure);
}
void UserInfoPopup::toggleLocalHighlight()
{
    // satorino: this isolated extra never calls the native API.
    const auto error = satorino::toggleLocalFlag(
        this->flagRoomId(), this->flagTargetId(),
        this->underlyingChannel_->getName(), this->userName_);
    if (!error.isEmpty())
        this->showFlagError(error);
}
}  // namespace chatterino
