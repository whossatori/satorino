// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"
#include "messages/Message.hpp"

#include <QDateTime>
#include <QString>

#include <optional>

namespace chatterino {

struct MentionReplyTarget {
    QString channelName;
    QString parentMessageId;
    QString parentLogin;
    QString parentDisplayName;
    QString parentDisplayText;
    MessagePlatform platform;
};

struct MentionReplyResult {
    std::optional<MentionReplyTarget> target;
    QString unavailableReason;
};

// Data-only validation; availability and time are explicit inputs for tests.
MentionReplyResult mentionReplyTarget(const Message &message,
                                      const QDateTime &now,
                                      bool originAvailable = true);

// GUI thread only. Lookup never joins/creates a channel or retains it.
ChannelPtr findMentionReplyChannel(const MentionReplyTarget &target);

}  // namespace chatterino
