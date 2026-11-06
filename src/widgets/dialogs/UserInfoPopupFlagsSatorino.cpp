// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/UserInfoPopup.hpp"
#include "controllers/flags/SatorinoFlags.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "messages/Message.hpp"
#include "singletons/Settings.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include <QCursor>
#include <QToolTip>
#include <QString>
#include <initializer_list>
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
QString UserInfoPopup::localFlagRoomId() const
{
    // satorino: the popup's message context, never the currently selected split.
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    return channel && channel->getType() == Channel::Type::Twitch &&
                   !this->isKick_ && !this->isYouTube_
               ? channel->roomId()
               : QString{};
}
void UserInfoPopup::bindLocalFlagButton()
{
    for (auto *button : {this->ui_.localFlag, this->ui_.localFlagModeration})
    {
        // LabelButton enables mouse tracking: Windows frameless hit-testing must return HTCLIENT.
        QObject::connect(button, &Button::leftClicked, this, [this] {
            this->toggleLocalFlag();
        });
    }
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
    if (!this->ui_.localFlag || !this->ui_.localFlagModeration)
        return;
    const auto channel =
        std::dynamic_pointer_cast<TwitchChannel>(this->underlyingChannel_);
    const bool real = channel && channel->getType() == Channel::Type::Twitch &&
                      !this->isKick_ && !this->isYouTube_;
    const bool inModeration =
        satorino::flagControlPlacement(this->shouldShowModerationActions()) ==
        satorino::FlagControlPlacement::ModerationBlock;
    this->ui_.localFlag->setVisible(real && !inModeration);
    this->ui_.localFlagModeration->setVisible(real && inModeration);
    const auto state = satorino::flagControlState(
        satorino::localFlags(), this->localFlagRoomId(),
        this->localFlagTargetId(),
        getSettings()->satorinoFlagsEnabled.getValue());
    for (auto *button : {this->ui_.localFlag, this->ui_.localFlagModeration})
    {
        button->setText(state.text);
        button->setEnabled(state.enabled);
        button->setToolTip(state.tooltip);
    }
}
void UserInfoPopup::toggleLocalFlag()
{
    const auto target = satorino::flagTarget(this->localFlagRoomId(),
                                             this->localFlagTargetId());
    const auto error =
        target ? satorino::toggleLocalFlag(target->roomId, target->userId,
                                           this->underlyingChannel_->getName(),
                                           this->userName_)
               : QString("Waiting for channel/user ID");
    // satorino: explicit feedback even if CompareBeforeSet suppresses a failed/no-op write.
    this->refreshLocalFlagButton();
    if (!error.isEmpty())
    {
        auto *button = this->ui_.localFlagModeration->isVisible()
                           ? this->ui_.localFlagModeration
                           : this->ui_.localFlag;
        button->setToolTip(error);
        QToolTip::showText(QCursor::pos(), error, button);
    }
}
}  // namespace chatterino
