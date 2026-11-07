// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/Message.hpp"
#include "util/SatorinoBadgeOrder.hpp"

#include <vector>

namespace chatterino {
class BadgeElement;
}
namespace chatterino::satorino {
QString badgeOrderKind(const BadgeElement &badge, MessagePlatform platform);
bool isOwnBadgeMessage(const Message &message);
std::vector<BadgeElement *> orderedBadgeElements(const Message &message,
                                                 const BadgeOrderRanks &ranks,
                                                 QStringView scope);
}  // namespace chatterino::satorino
