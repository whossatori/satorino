// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/EmotePopupGroups.hpp"

#include "Test.hpp"

#include <QString>
#include <QStringView>

#include <algorithm>
#include <array>
#include <vector>

using namespace chatterino::satorino;
using namespace Qt::StringLiterals;

namespace {
const std::array<QString, 2> LABELS{u"Emotes"_s, u"Zero-width emotes"_s};

struct Entry {
    int id;
    QString name;
    bool zeroWidth;
};

const std::vector<Entry> ENTRIES{
    {1, u"a-overlay"_s, true},
    {2, u"b-normal"_s, false},
    {3, u"c-overlay"_s, true},
    {4, u"d-normal"_s, false},
};

auto groupFor(const Entry &entry)
{
    return entry.zeroWidth ? 1U : 0U;
}

bool all(const Entry &)
{
    return true;
}
}  // namespace

TEST(EmotePopupGroups, EmptyInputHasNoHeaders)
{
    EXPECT_TRUE(
        groupEmotePopup(std::vector<Entry>{}, LABELS, groupFor, all).empty());
}

TEST(EmotePopupGroups, StablePartitionConservesAllEntries)
{
    const auto groups = groupEmotePopup(ENTRIES, LABELS, groupFor, all);
    ASSERT_EQ(groups.size(), 2U);
    EXPECT_EQ(groups[0].title, LABELS[0]);
    EXPECT_EQ(groups[1].title, LABELS[1]);
    ASSERT_EQ(groups[0].emotes.size(), 2U);
    ASSERT_EQ(groups[1].emotes.size(), 2U);
    EXPECT_EQ(groups[0].emotes[0].id, 2);
    EXPECT_EQ(groups[0].emotes[1].id, 4);
    EXPECT_EQ(groups[1].emotes[0].id, 1);
    EXPECT_EQ(groups[1].emotes[1].id, 3);
    std::vector<int> ids;
    for (const auto &group : groups)
    {
        for (const auto &emote : group.emotes)
        {
            ids.push_back(emote.id);
        }
    }
    std::ranges::sort(ids);
    EXPECT_EQ(ids, (std::vector<int>{1, 2, 3, 4}));
}

TEST(EmotePopupGroups, EmptySubsectionsAreOmitted)
{
    for (const auto &entry : ENTRIES)
    {
        const auto groups =
            groupEmotePopup(std::vector<Entry>{entry}, LABELS, groupFor, all);
        ASSERT_EQ(groups.size(), 1U);
        EXPECT_EQ(groups.front().title, LABELS[entry.zeroWidth ? 1U : 0U]);
        ASSERT_EQ(groups.front().emotes.size(), 1U);
        EXPECT_EQ(groups.front().emotes.front().id, entry.id);
    }
}

TEST(EmotePopupGroups, SearchKeepsGroupAndSurvivingEntryOrder)
{
    const auto groups =
        groupEmotePopup(ENTRIES, LABELS, groupFor, [](const Entry &entry) {
            return entry.name.contains(u"overlay", Qt::CaseInsensitive) ||
                   entry.id == 4;
        });
    ASSERT_EQ(groups.size(), 2U);
    EXPECT_EQ(groups[0].title, LABELS[0]);
    EXPECT_EQ(groups[1].title, LABELS[1]);
    ASSERT_EQ(groups[0].emotes.size(), 1U);
    EXPECT_EQ(groups[0].emotes.front().id, 4);
    ASSERT_EQ(groups[1].emotes.size(), 2U);
    EXPECT_EQ(groups[1].emotes[0].id, 1);
    EXPECT_EQ(groups[1].emotes[1].id, 3);
}

TEST(EmotePopupGroups, SearchHidesEmptySubsectionsAndAllEmptyResults)
{
    const auto overlayOnly =
        groupEmotePopup(ENTRIES, LABELS, groupFor, [](const Entry &entry) {
            return entry.name.contains(u"OVERLAY", Qt::CaseInsensitive);
        });
    ASSERT_EQ(overlayOnly.size(), 1U);
    EXPECT_EQ(overlayOnly.front().title, LABELS[1]);
    EXPECT_TRUE(groupEmotePopup(ENTRIES, LABELS, groupFor, [](const Entry &) {
                    return false;
                }).empty());
}

// satorino: API types, not emote IDs/names or arbitrary non-subscription fallback.
TEST(EmotePopupGroups, TwitchTypesUseOnlyDocumentedPositiveClassifications)
{
    EXPECT_EQ(twitchPopupEmoteCategory(u"globals"),
              TwitchPopupEmoteCategory::Default);
    EXPECT_EQ(twitchPopupEmoteCategory(u"smilies"),
              TwitchPopupEmoteCategory::Default);
    EXPECT_EQ(twitchPopupEmoteCategory(u"hypetrain"),
              TwitchPopupEmoteCategory::HypeTrain);
    const std::array<QString, 13> otherTypes{
        u""_s,           u"none"_s,          u"bitstier"_s,
        u"follower"_s,   u"subscriptions"_s, u"channelpoints"_s,
        u"rewards"_s,    u"prime"_s,         u"turbo"_s,
        u"owl2019"_s,    u"twofactor"_s,     u"limitedtime"_s,
        u"future-type"_s};
    for (const auto &type : otherTypes)
    {
        EXPECT_EQ(twitchPopupEmoteCategory(type),
                  TwitchPopupEmoteCategory::Other);
    }
}

TEST(EmotePopupGroups, TwitchPartitionConservesUnknownAndRestrictedTypes)
{
    // These are synthetic type strings, never IDs or guessed emote-name prefixes.
    const std::vector<QString> types{u"prime"_s,   u"hypetrain"_s, u"globals"_s,
                                     u"rewards"_s, u"smilies"_s,   u""_s};
    const auto groups = groupEmotePopup(
        types, twitchPopupEmoteLabels(),
        [](const QString &type) {
            return twitchPopupEmoteCategory(type);
        },
        [](const QString &) {
            return true;
        });
    ASSERT_EQ(groups.size(), 3U);
    EXPECT_EQ(groups[0].title, u"Default emotes"_s);
    EXPECT_EQ(groups[1].title, u"Hype Train emotes"_s);
    EXPECT_EQ(groups[2].title, u"Other emotes"_s);
    EXPECT_EQ(groups[0].emotes,
              (std::vector<QString>{u"globals"_s, u"smilies"_s}));
    EXPECT_EQ(groups[1].emotes, (std::vector<QString>{u"hypetrain"_s}));
    EXPECT_EQ(groups[2].emotes,
              (std::vector<QString>{u"prime"_s, u"rewards"_s, u""_s}));
    auto allTypes = groups[0].emotes;
    allTypes.insert(allTypes.end(), groups[1].emotes.begin(),
                    groups[1].emotes.end());
    allTypes.insert(allTypes.end(), groups[2].emotes.begin(),
                    groups[2].emotes.end());
    auto expected = types;
    std::ranges::sort(expected);
    std::ranges::sort(allTypes);
    EXPECT_EQ(allTypes, expected);
}

TEST(EmotePopupGroups, TwitchSearchHidesEmptyCategoriesWithoutRelabelling)
{
    const std::vector<QString> types{u"hypetrain"_s, u"globals"_s, u"prime"_s,
                                     u"smilies"_s};
    const auto groups = groupEmotePopup(
        types, twitchPopupEmoteLabels(),
        [](const QString &type) {
            return twitchPopupEmoteCategory(type);
        },
        [](const QString &type) {
            return type != u"hypetrain";
        });
    ASSERT_EQ(groups.size(), 2U);
    EXPECT_EQ(groups[0].title, u"Default emotes"_s);
    EXPECT_EQ(groups[1].title, u"Other emotes"_s);
    EXPECT_EQ(groups[0].emotes,
              (std::vector<QString>{u"globals"_s, u"smilies"_s}));
    EXPECT_EQ(groups[1].emotes, (std::vector<QString>{u"prime"_s}));
}
