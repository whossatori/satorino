// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoBadgeOrder.hpp"

#include <QSet>

#include <limits>

namespace chatterino::satorino {
using namespace Qt::StringLiterals;

const std::vector<BadgeKindInfo> &badgeKinds()
{
    // satorino: kinds derive from TwitchBadge, BadgeHighlight and provider flags.
    // This is a UI catalogue, not a replacement for the variable native message order.
    static const std::vector<BadgeKindInfo> kinds{
        {u"broadcaster"_s, u"Broadcaster"_s},
        {u"global-authority"_s, u"Staff / admin / global moderator"_s},
        {u"moderator"_s, u"Moderator"_s},
        {u"vip"_s, u"VIP"_s},
        {u"subscription"_s, u"Founder / subscriber"_s},
        {u"bits"_s, u"Bits"_s},
        {u"predictions"_s, u"Predictions"_s},
        {u"twitch-other"_s, u"Other Twitch badges"_s},
        {u"ffz-mod"_s, u"FFZ moderator"_s},
        {u"ffz-vip"_s, u"FFZ VIP"_s},
        {u"chatterino"_s, u"Chatterino"_s},
        {u"ffz-user"_s, u"FFZ user"_s},
        {u"ffz-ap"_s, u"FFZ Add-On Pack"_s},
        {u"bttv"_s, u"BTTV"_s},
        {u"seventv"_s, u"7TV"_s},
        {u"dankchat"_s, u"DankChat"_s},
        {u"chatsen"_s, u"Chatsen"_s},
        {u"bluzyrino"_s, u"Bluzyrino"_s},
        {u"jilchat"_s, u"JilChat"_s},
        {u"badgehq"_s, u"BadgeHQ"_s},
        {u"limerino"_s, u"Limerino"_s},
        {u"homies"_s, u"Homies"_s},
        {u"folhinha"_s, u"Folhinha"_s},
        {u"other"_s, u"Other badges"_s},
    };
    return kinds;
}

QStringList defaultBadgeKindOrder()
{
    QStringList order;
    for (const auto &kind : badgeKinds())
        order.push_back(kind.key);
    return order;
}

QStringList normalizeBadgeKindOrder(const QStringList &saved)
{
    const auto defaults = defaultBadgeKindOrder();
    QStringList result;
    QSet<QString> seen;
    for (const auto &key : saved)
        if (defaults.contains(key) && !seen.contains(key))
        {
            result.push_back(key);
            seen.insert(key);
        }
    for (const auto &key : defaults)
        if (!seen.contains(key))
            result.push_back(key);
    return result;
}

BadgeOrderRanks makeBadgeOrderRanks(const QStringList &saved)
{
    BadgeOrderRanks result;
    const auto defaults = defaultBadgeKindOrder();
    result.custom = std::ranges::any_of(saved, [&](const QString &key) {
        return defaults.contains(key);
    });
    if (result.custom)
    {
        int rank = 0;
        for (const auto &key : normalizeBadgeKindOrder(saved))
            result.ranks.insert(key, rank++);
    }
    return result;
}

int BadgeOrderRanks::rank(const QString &kind) const
{
    return this->ranks.value(kind, std::numeric_limits<int>::max());
}

bool badgeOrderApplies(QStringView scope, bool isOwnMessage, bool isTwitch)
{
    return isTwitch && (scope != u"only-me" || isOwnMessage);
}

BadgeIconChoice badgeIconChoice(bool hasVisiblePixels, int width, int height)
{
    return hasVisiblePixels && width > 0 && height > 0
               ? BadgeIconChoice::Real
               : BadgeIconChoice::Placeholder;
}

bool badgeIconInPreview(BadgeIconChoice choice)
{
    return choice == BadgeIconChoice::Real;
}

bool badgeOrderOwnAuthor(QStringView author, QStringView current, bool loggedIn)
{
    return loggedIn && !author.isEmpty() && !current.isEmpty() &&
           author != u"0" && current != u"0" && author == current;
}

QString twitchBadgeOrderKind(QStringView slug)
{
    if (slug == u"broadcaster")
        return u"broadcaster"_s;
    if (slug == u"staff" || slug == u"admin" || slug == u"global_mod")
        return u"global-authority"_s;
    if (slug == u"moderator" || slug == u"lead_moderator")
        return u"moderator"_s;
    if (slug == u"vip")
        return u"vip"_s;
    if (slug == u"founder" || slug == u"subscriber")
        return u"subscription"_s;
    // Existing local twitch-badges.json set keys, not individual badge IDs.
    if (slug == u"bits" || slug == u"bits-charity" || slug == u"bits-leader")
        return u"bits"_s;
    if (slug == u"predictions")
        return u"predictions"_s;
    return slug.isEmpty() ? QString{} : u"twitch-other"_s;
}

}  // namespace chatterino::satorino
