// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/UserInfoPopup.hpp"
#include "controllers/flags/SatorinoFlags.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "messages/Message.hpp"
#include "singletons/Settings.hpp"
#include <QPushButton>
#include <QSignalBlocker>
#include <QWidget>
#include <memory>

namespace chatterino {
QString UserInfoPopup::localFlagTargetId() const
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
void UserInfoPopup::bindLocalFlagButton()
{
    this->ui_.localFlag->setCheckable(true);
    this->ui_.localFlag->setMaximumHeight(26);
    QObject::connect(this->ui_.localFlag, &QPushButton::clicked, this, [this] {
        this->toggleLocalFlag();
    });
    getSettings()->satorinoFlagsState.connect(
        [this](const QString &) {
            this->refreshLocalFlagButton();
        },
        this->signalHolder_, false);
    getSettings()->satorinoFlagsEnabled.connect(
        [this](bool) {
            this->refreshLocalFlagButton();
        },
        this->signalHolder_, false);
    this->refreshLocalFlagButton();
}
void UserInfoPopup::refreshLocalFlagButton()
{
    if (!this->ui_.localFlag)
        return;
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    const bool real = channel && channel->getType() == Channel::Type::Twitch &&
                      !this->isKick_ && !this->isYouTube_;
    this->ui_.localFlag->setVisible(real);
    const auto room = real ? channel->roomId() : QString{};
    const auto id = this->localFlagTargetId();
    const bool ready =
        real && satorino::validFlagId(room) && satorino::validFlagId(id);
    const bool flagged = ready && satorino::localFlags().contains(room, id);
    const QSignalBlocker blocker(this->ui_.localFlag);
    this->ui_.localFlag->setChecked(flagged);
    this->ui_.localFlag->setText(flagged ? "Unflag user" : "Flag user");
    this->ui_.localFlag->setEnabled(
        ready && getSettings()->satorinoFlagsEnabled.getValue());
    this->ui_.localFlag->setToolTip(
        !getSettings()->satorinoFlagsEnabled.getValue() ? "Local flags are off"
        : !ready ? "Waiting for channel/user ID"
                 : "Local to this channel; nothing is sent to Twitch");
}
void UserInfoPopup::toggleLocalFlag()
{
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    if (!channel || channel->getType() != Channel::Type::Twitch)
        return;
    const auto error =
        satorino::toggleLocalFlag(channel->roomId(), this->localFlagTargetId(),
                                  channel->getName(), this->userName_);
    this->refreshLocalFlagButton();
    if (!error.isEmpty())
        this->ui_.localFlag->setToolTip(error);
}
}  // namespace chatterino
