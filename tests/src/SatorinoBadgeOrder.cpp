// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoBadgeOrder.hpp"
#include "controllers/badges/SatorinoBadgeOrder.hpp"
#include "messages/Emote.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"
#include "Test.hpp"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

using namespace chatterino;
using namespace chatterino::satorino;
using namespace Qt::StringLiterals;
namespace {
struct Entry {
    int id;
    QString kind;
};
const std::vector<Entry> ENTRIES{{1, u"seventv"_s},   {2, u"moderator"_s},
                                 {3, u"ffz-user"_s},  {4, u"broadcaster"_s},
                                 {5, u"unknown-a"_s}, {6, u"unknown-b"_s},
                                 {7, u"moderator"_s}};
std::vector<int> sortedIds(const QStringList &saved)
{
    const auto result = stableBadgeOrder(ENTRIES, makeBadgeOrderRanks(saved),
                                         [](const Entry &e) -> const QString & {
                                             return e.kind;
                                         });
    std::vector<int> ids;
    for (const auto &entry : result)
        ids.push_back(entry.id);
    return ids;
}
EmotePtr sample(const QString &name = {})
{
    auto emote = std::make_shared<Emote>();
    emote->name = EmoteName{name};
    return emote;
}
BadgeElement *addBadge(Message &message, MessageElementFlag flag)
{
    auto badge = std::make_unique<BadgeElement>(sample(), flag);
    auto *ptr = badge.get();
    message.elements.push_back(std::move(badge));
    return ptr;
}
}  // namespace

TEST(SatorinoBadgeOrder, EmptyDefaultPreservesExactOriginalOrder)
{
    EXPECT_FALSE(makeBadgeOrderRanks({}).custom);
    EXPECT_EQ(sortedIds({}), (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));
}
TEST(SatorinoBadgeOrder, UserOrderAppliesByKindAndStaysStable)
{
    EXPECT_EQ(sortedIds({u"moderator"_s, u"seventv"_s, u"ffz-user"_s,
                         u"broadcaster"_s}),
              (std::vector<int>{2, 7, 1, 3, 4, 5, 6}));
}
TEST(SatorinoBadgeOrder, UnknownKindsAreLastInOriginalOrder)
{
    const auto ids = sortedIds({u"seventv"_s});
    ASSERT_EQ(ids.size(), ENTRIES.size());
    EXPECT_EQ(ids[ids.size() - 2], 5);
    EXPECT_EQ(ids.back(), 6);
}
TEST(SatorinoBadgeOrder, DuplicateAndGarbageSavedKeysAreIgnored)
{
    EXPECT_EQ(normalizeBadgeKindOrder({u"moderator"_s, u"garbage"_s, u""_s,
                                       u"moderator"_s, u"seventv"_s}),
              normalizeBadgeKindOrder({u"moderator"_s, u"seventv"_s}));
}
TEST(SatorinoBadgeOrder, EntirelyInvalidOrderFallsBackToNative)
{
    EXPECT_EQ(sortedIds({u"future-kind"_s, u""_s, u" garbage "_s}),
              (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));
}
TEST(SatorinoBadgeOrder, MissingKnownKindsAreAppendedOnce)
{
    const auto order = normalizeBadgeKindOrder({u"seventv"_s});
    EXPECT_EQ(order.front(), u"seventv"_s);
    EXPECT_EQ(order.size(), defaultBadgeKindOrder().size());
    for (const auto &key : defaultBadgeKindOrder())
        EXPECT_EQ(order.count(key), 1);
}
TEST(SatorinoBadgeOrder, NoBadgeLostOrDuplicated)
{
    auto ids = sortedIds({u"ffz-user"_s, u"broadcaster"_s});
    std::ranges::sort(ids);
    EXPECT_EQ(ids, (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));
}
TEST(SatorinoBadgeOrder, EveryoneIncludesOwnAndOthers)
{
    EXPECT_TRUE(badgeOrderApplies(u"everyone", true, true));
    EXPECT_TRUE(badgeOrderApplies(u"everyone", false, true));
}
TEST(SatorinoBadgeOrder, OnlyMeIncludesOnlyOwn)
{
    EXPECT_TRUE(badgeOrderApplies(u"only-me", true, true));
    EXPECT_FALSE(badgeOrderApplies(u"only-me", false, true));
}
TEST(SatorinoBadgeOrder, UnknownSavedScopeFallsBackToEveryone)
{
    EXPECT_TRUE(badgeOrderApplies(u"future-scope", false, true));
    EXPECT_TRUE(badgeOrderApplies(u"", false, true));
}
TEST(SatorinoBadgeOrder, OwnAuthorUsesNonemptyLoggedInIdsOnly)
{
    EXPECT_TRUE(badgeOrderOwnAuthor(u"123", u"123", true));
    EXPECT_FALSE(badgeOrderOwnAuthor(u"456", u"123", true));
    EXPECT_FALSE(badgeOrderOwnAuthor(u"123", u"123", false));
    EXPECT_FALSE(badgeOrderOwnAuthor(u"", u"123", true));
    EXPECT_FALSE(badgeOrderOwnAuthor(u"123", u"", true));
    EXPECT_FALSE(badgeOrderOwnAuthor(u"0", u"0", true));
    EXPECT_FALSE(badgeOrderApplies(u"only-me",
                                   badgeOrderOwnAuthor(u"", u"", false), true));
    EXPECT_FALSE(badgeOrderApplies(
        u"only-me", badgeOrderOwnAuthor(u"", u"123", true), true));
}
TEST(SatorinoBadgeOrder, ExistingNativeSlugsMapToKinds)
{
    EXPECT_EQ(twitchBadgeOrderKind(u"broadcaster"), u"broadcaster"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"staff"), u"global-authority"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"admin"), u"global-authority"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"global_mod"), u"global-authority"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"lead_moderator"), u"moderator"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"vip"), u"vip"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"founder"), u"subscription"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"subscriber"), u"subscription"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"bits"), u"bits"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"bits-charity"), u"bits"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"bits-leader"), u"bits"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"predictions"), u"predictions"_s);
    EXPECT_EQ(twitchBadgeOrderKind(u"future-badge"), u"twitch-other"_s);
    EXPECT_TRUE(twitchBadgeOrderKind(u"").isEmpty());
}
TEST(SatorinoBadgeOrder, NativeLayoutBypassDoesNotNeedAnAccount)
{
    Message message;
    addBadge(message, MessageElementFlag::BadgeChatterino);
    EXPECT_TRUE(
        orderedBadgeElements(message, makeBadgeOrderRanks({}), u"only-me")
            .empty());
}
TEST(SatorinoBadgeOrder,
     LayoutReturnsOriginalElementsWithoutChangingFlagsOrOwnership)
{
    Message message;
    auto *ffz = addBadge(message, MessageElementFlag::BadgeFfz);
    auto *chat = addBadge(message, MessageElementFlag::BadgeChatterino);
    auto *bttv = addBadge(message, MessageElementFlag::BadgeBttv);
    ffz->setTooltip(u"same tooltip"_s);
    const auto order = orderedBadgeElements(
        message,
        makeBadgeOrderRanks({u"bttv"_s, u"chatterino"_s, u"ffz-user"_s}),
        u"everyone");
    EXPECT_EQ(order, (std::vector<BadgeElement *>{bttv, chat, ffz}));
    EXPECT_EQ(message.elements[0].get(), ffz);
    EXPECT_EQ(message.elements[1].get(), chat);
    EXPECT_EQ(message.elements[2].get(), bttv);
    EXPECT_EQ(ffz->getTooltip(), u"same tooltip"_s);
    EXPECT_TRUE(ffz->getFlags().has(MessageElementFlag::BadgeFfz));
}
TEST(SatorinoBadgeOrder, LayoutKeepsTextBoundariesAndStopsAtAuthor)
{
    Message message;
    auto *ffz = addBadge(message, MessageElementFlag::BadgeFfz);
    message.elements.push_back(std::make_unique<TextElement>(
        u"separator"_s, MessageElementFlag::Text));
    auto *chat = addBadge(message, MessageElementFlag::BadgeChatterino);
    auto *bttv = addBadge(message, MessageElementFlag::BadgeBttv);
    message.elements.push_back(std::make_unique<TextElement>(
        u"author"_s, MessageElementFlag::Username));
    addBadge(message, MessageElementFlag::BadgeSevenTV);
    EXPECT_EQ(
        orderedBadgeElements(
            message,
            makeBadgeOrderRanks({u"bttv"_s, u"chatterino"_s, u"ffz-user"_s}),
            u"everyone"),
        (std::vector<BadgeElement *>{ffz, bttv, chat}));
}
TEST(SatorinoBadgeOrder, FfzRoleReplacementsRemainDistinctKinds)
{
    ModBadgeElement mod(sample(), MessageElementFlag::BadgeChannelAuthority);
    mod.setTwitchBadge(u"moderator"_s, u"1"_s);
    VipBadgeElement vip(sample(), MessageElementFlag::BadgeChannelAuthority);
    vip.setTwitchBadge(u"vip"_s, u"1"_s);
    EXPECT_EQ(badgeOrderKind(mod), u"ffz-mod"_s);
    EXPECT_EQ(badgeOrderKind(vip), u"ffz-vip"_s);
}
// Batch 19 classified Kick roles; Batch 20 explicitly preserves their native order.
TEST(SatorinoBadgeOrder, KickMessagesAlwaysKeepNativeOrder)
{
    Message message;
    message.platform = MessagePlatform::Kick;
    message.userID = u"123"_s;
    auto *ffz = addBadge(message, MessageElementFlag::BadgeFfz);
    auto *chat = addBadge(message, MessageElementFlag::BadgeChatterino);
    const auto ranks = makeBadgeOrderRanks({u"chatterino"_s, u"ffz-user"_s});
    for (const auto &scope : {u"everyone"_s, u"only-me"_s, u"unknown"_s})
    {
        EXPECT_TRUE(orderedBadgeElements(message, ranks, scope).empty());
        EXPECT_EQ(message.elements[0].get(), ffz);
        EXPECT_EQ(message.elements[1].get(), chat);
    }
    EXPECT_FALSE(
        isOwnBadgeMessage(message));  // no Kick account lookup, even with an ID
}

TEST(SatorinoBadgeOrder, UnknownOwnAuthorBypassesCustomLayoutSort)
{
    Message message;
    addBadge(message, MessageElementFlag::BadgeChatterino);
    EXPECT_TRUE(orderedBadgeElements(
                    message, makeBadgeOrderRanks({u"chatterino"_s}), u"only-me")
                    .empty());
}

TEST(SatorinoBadgeOrder,
     ReplyBadgesKeepTheirOwnPositionAndDoNotConsumeAuthorOrder)
{
    Message message;
    auto reply = std::make_unique<BadgeElement>(
        sample(), MessageElementFlags{MessageElementFlag::BadgeFfz,
                                      MessageElementFlag::RepliedMessage});
    auto *replyBadge = reply.get();
    message.elements.push_back(std::move(reply));
    auto *chat = addBadge(message, MessageElementFlag::BadgeChatterino);
    auto *bttv = addBadge(message, MessageElementFlag::BadgeBttv);
    EXPECT_EQ(orderedBadgeElements(message, makeBadgeOrderRanks({u"bttv"_s}),
                                   u"everyone"),
              (std::vector<BadgeElement *>{bttv, chat}));
    EXPECT_EQ(message.elements.front().get(), replyBadge);
}

TEST(SatorinoBadgeOrder, NonTwitchScopeNeverApplies)
{
    for (const auto &scope : {u"everyone"_s, u"only-me"_s, u"unknown"_s})
        for (const bool own : {false, true})
            EXPECT_FALSE(badgeOrderApplies(scope, own, false));
}

TEST(SatorinoBadgeOrder, OtherPlatformsAlsoKeepNativeOrder)
{
    Message message;
    message.platform = MessagePlatform::YouTube;
    message.userID = u"123"_s;
    addBadge(message, MessageElementFlag::BadgeChatterino);
    EXPECT_TRUE(orderedBadgeElements(message,
                                     makeBadgeOrderRanks({u"chatterino"_s}),
                                     u"everyone")
                    .empty());
    EXPECT_FALSE(isOwnBadgeMessage(message));
}

TEST(SatorinoBadgeOrder, RealVisibleImageBeatsPlaceholder)
{
    EXPECT_EQ(badgeIconChoice(true, 22, 22), BadgeIconChoice::Real);
    EXPECT_TRUE(badgeIconInPreview(badgeIconChoice(true, 18, 36)));
}

TEST(SatorinoBadgeOrder, MissingOrTransparentImageUsesPlaceholder)
{
    EXPECT_EQ(badgeIconChoice(false, 22, 22), BadgeIconChoice::Placeholder);
    EXPECT_EQ(badgeIconChoice(false, 0, 0), BadgeIconChoice::Placeholder);
}

TEST(SatorinoBadgeOrder, ZeroOrInvalidSizeUsesPlaceholder)
{
    EXPECT_EQ(badgeIconChoice(true, 0, 22), BadgeIconChoice::Placeholder);
    EXPECT_EQ(badgeIconChoice(true, 22, 0), BadgeIconChoice::Placeholder);
    EXPECT_EQ(badgeIconChoice(true, -1, 22), BadgeIconChoice::Placeholder);
}

TEST(SatorinoBadgeOrder, PreviewOmitsEntriesWithoutRealPixels)
{
    EXPECT_FALSE(badgeIconInPreview(badgeIconChoice(false, 22, 22)));
    EXPECT_FALSE(badgeIconInPreview(badgeIconChoice(true, 0, 22)));
    EXPECT_TRUE(badgeIconInPreview(badgeIconChoice(true, 22, 22)));
}
