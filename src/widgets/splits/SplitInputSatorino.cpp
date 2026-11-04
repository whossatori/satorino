// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitInput.hpp"

#include "util/MentionReply.hpp"
#include "widgets/splits/Split.hpp"

namespace chatterino {

ChannelPtr SplitInput::resolveMentionReplyForSend(const MessagePtr &message)
{
    auto result = mentionReplyTarget(*message, QDateTime::currentDateTime());
    if (result.target)
    {
        if (auto channel = findMentionReplyChannel(*result.target))
        {
            return channel;
        }
        result =
            mentionReplyTarget(*message, QDateTime::currentDateTime(), false);
    }
    this->split_->getChannel()->addSystemMessage(result.unavailableReason);
    return {};
}

}  // namespace chatterino
