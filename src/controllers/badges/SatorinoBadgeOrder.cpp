// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/badges/SatorinoBadgeOrder.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/TwitchAccount.hpp"

#include <utility>

namespace chatterino::satorino {
using namespace Qt::StringLiterals;

QString badgeOrderKind(const BadgeElement &badge)
{
    if (badge.type() == ModBadgeElement::TYPE)
        return u"ffz-mod"_s;
    if (badge.type() == VipBadgeElement::TYPE)
        return u"ffz-vip"_s;
    if (const auto slug = badge.twitchBadgeSlug())
        return twitchBadgeOrderKind(*slug);
    const auto flags = badge.getFlags();
    if (flags.has(MessageElementFlag::BadgeChatterino))
        return u"chatterino"_s;
    if (flags.has(MessageElementFlag::BadgeFfz))
        return u"ffz-user"_s;
    if (flags.has(MessageElementFlag::BadgeFfzAp))
        return u"ffz-ap"_s;
    if (flags.has(MessageElementFlag::BadgeBttv))
        return u"bttv"_s;
    if (flags.has(MessageElementFlag::BadgeSevenTV))
        return u"seventv"_s;
    if (flags.has(MessageElementFlag::BadgeDankChat))
        return u"dankchat"_s;
    if (flags.has(MessageElementFlag::BadgeChatsen))
        return u"chatsen"_s;
    if (flags.has(MessageElementFlag::BadgeBluzyrino))
        return u"bluzyrino"_s;
    if (flags.has(MessageElementFlag::BadgeJilChat))
        return u"jilchat"_s;
    if (flags.has(MessageElementFlag::BadgeBadgeHQ))
        return u"badgehq"_s;
    if (flags.has(MessageElementFlag::BadgeLimerino))
        return u"limerino"_s;
    if (flags.hasAny(MessageElementFlag::BadgeHomies,
                     MessageElementFlag::BadgeHomiesCustom,
                     MessageElementFlag::BadgeHomiesSupporter))
        return u"homies"_s;
    if (flags.has(MessageElementFlag::BadgeFolhinha))
        return u"folhinha"_s;
    if (flags.hasAny(MessageElementFlag::BadgeVanity,
                     MessageElementFlag::BadgeSharedChannel,
                     MessageElementFlag::BadgeMoltorino))
        return u"other"_s;
    return {};  // unclassified kinds always follow the user's known kinds, stably
}

bool isOwnBadgeMessage(const Message &message)
{
    // satorino: no account lookup or name fallback for non-Twitch messages.
    if (message.platform != MessagePlatform::AnyOrTwitch ||
        message.userID.isEmpty() || message.userID == u"0")
        return false;
    auto *app = tryGetApp();
    auto *accounts = app ? app->getAccounts() : nullptr;
    if (!accounts)
        return false;
    // Same account ID check as Split::Split's self-message pulse, without unknown IDs.
    const auto account = accounts->twitch.getCurrent();
    return account && badgeOrderOwnAuthor(message.userID, account->getUserId(),
                                          !account->isAnon());
}

std::vector<BadgeElement *> orderedBadgeElements(const Message &message,
                                                 const BadgeOrderRanks &ranks,
                                                 QStringView scope)
{
    // Default and ineligible messages do not scan or sort their badge elements.
    if (!ranks.custom ||
        !badgeOrderApplies(scope,
                           scope == u"only-me" && isOwnBadgeMessage(message),
                           message.platform == MessagePlatform::AnyOrTwitch))
        return {};

    std::vector<BadgeElement *> ordered;
    std::vector<BadgeElement *> run;
    const auto flush = [&] {
        auto sorted =
            stableBadgeOrder(std::move(run), ranks, [&](BadgeElement *badge) {
                return badgeOrderKind(*badge);
            });
        ordered.insert(ordered.end(), sorted.begin(), sorted.end());
        run.clear();
    };
    for (const auto &element : message.elements)
    {
        if (element->getFlags().has(MessageElementFlag::RepliedMessage))
        {
            if (!run.empty())
                flush();
            continue;
        }
        // Only the author's badge prefix; never move reply/body badges across text.
        if (element->getFlags().hasAny(MessageElementFlag::Username,
                                       MessageElementFlag::KickUsername) &&
            !element->getFlags().has(MessageElementFlag::RepliedMessage))
            break;
        if (auto *badge = dynamic_cast<BadgeElement *>(element.get()))
            run.push_back(badge);
        else if (!run.empty())
            flush();
    }
    if (!run.empty())
        flush();
    return ordered;
}

}  // namespace chatterino::satorino
