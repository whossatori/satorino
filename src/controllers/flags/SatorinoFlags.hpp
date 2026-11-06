// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include "util/SatorinoFlags.hpp"
namespace chatterino {
class Channel;
struct Message;
namespace satorino {
const FlagStore &localFlags();
QString localFlagRoom(const Message &message, Channel *context);
FlagStyle localFlagStyle(const Message &message, const QString &room,
                         bool ignoreHighlights);
QString toggleLocalFlag(const QString &room, const QString &user,
                        const QString &channelLabel, const QString &userLabel);
void removeLocalFlag(const QString &room, const QString &user);
}  // namespace satorino
}  // namespace chatterino
