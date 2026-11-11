// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "providers/satorino/SatorinoBadges.hpp"

#include "messages/Image.hpp"
#include "Test.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace chatterino;

TEST(SatorinoBadges, ResolvesActiveBadgesByTwitchId)
{
    const auto badges = satorino::parseBadgeFeed(R"json({
        "version": 1,
        "badges": {
            "owner": {
                "name": "Owner / Dev",
                "image1x": "https://satorino.satos.cc/badges/owner-1x.png?v=2",
                "image2x": "https://satorino.satos.cc/badges/owner-2x.png?v=2",
                "image3x": "https://satorino.satos.cc/badges/owner-3x.png?v=2"
            },
            "tier5": {
                "name": "Supporter <5>",
                "image1x": "https://satorino.satos.cc/badges/tier5-1x.png"
            }
        },
        "users": {
            "812407555": "owner",
            "9007199254740993": "tier5",
            "9007199254740994": "tier5",
            "9007199254740995": "none",
            "9007199254740996": "unknown",
            "satxris": "owner",
            "0": "owner"
        }
    })json");

    ASSERT_TRUE(badges);
    ASSERT_EQ(badges->size(), 3U);
    ASSERT_TRUE(badges->contains("812407555"));
    ASSERT_TRUE(badges->contains("9007199254740993"));
    ASSERT_TRUE(badges->contains("9007199254740994"));
    const auto &owner = badges->at("812407555").front();
    EXPECT_EQ(owner->name.string, "satorino:owner");
    EXPECT_EQ(owner->tooltip.string, "Owner / Dev");
    EXPECT_EQ(owner->images.getImage1()->scale(), 1.0);
    EXPECT_EQ(owner->images.getImage2()->scale(), 0.5);
    EXPECT_EQ(owner->images.getImage3()->scale(), 0.25);
    EXPECT_EQ(owner->images.getImage3()->url().string,
              "https://satorino.satos.cc/badges/owner-3x.png?v=2");
    const auto &supporter = badges->at("9007199254740993").front();
    EXPECT_EQ(supporter, badges->at("9007199254740994").front());
    EXPECT_EQ(supporter->tooltip.string, "Supporter &lt;5&gt;");
    EXPECT_TRUE(supporter->images.getImage2()->isEmpty());
}

TEST(SatorinoBadges, RejectsInvalidFeedsAndAcceptsEmptyAssignments)
{
    EXPECT_FALSE(satorino::parseBadgeFeed("invalid JSON"));
    EXPECT_FALSE(
        satorino::parseBadgeFeed(R"({"version":3,"badges":{},"users":{}})"));
    EXPECT_FALSE(
        satorino::parseBadgeFeed(R"({"version":1,"badges":{},"users":[]})"));
    const auto empty =
        satorino::parseBadgeFeed(R"({"version":1,"badges":{},"users":{}})");
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
}

TEST(SatorinoBadges, RejectsImagesOutsideThePublicHttpsOrigin)
{
    const auto badges = satorino::parseBadgeFeed(R"json({
        "version": 1,
        "badges": {
            "otherHost": {
                "name": "Other host",
                "image1x": "https://other.example/badge.png"
            },
            "local": {
                "name": "Local file",
                "image1x": "file:///tmp/badge.png"
            },
            "insecure": {
                "name": "HTTP",
                "image1x": "http://satorino.satos.cc/badge.png"
            },
            "valid": {
                "name": "Valid",
                "image1x": "https://satorino.satos.cc/badges/founder-1x.png"
            }
        },
        "users": {"1":"otherHost","2":"local","3":"insecure","4":"valid"}
    })json");
    ASSERT_TRUE(badges);
    ASSERT_EQ(badges->size(), 1U);
    EXPECT_TRUE(badges->contains("4"));
}

TEST(SatorinoBadges, VersionTwoPreservesSelectionOrderAndSkipsInvalidIds)
{
    const auto badges = satorino::parseBadgeFeed(R"json({
        "version": 2,
        "badges": {
            "owner": {"name":"Owner","image1x":"https://satorino.satos.cc/badges/owner-1x.png"},
            "founder": {"name":"Founder","image1x":"https://satorino.satos.cc/badges/founder-1x.png"},
            "tier5": {"name":"Tier 5","image1x":"https://satorino.satos.cc/badges/tier5-1x.png"},
            "tier10": {"name":"Tier 10","image1x":"https://satorino.satos.cc/badges/tier10-1x.png"},
            "tier20": {"name":"Tier 20","image1x":"https://satorino.satos.cc/badges/tier20-1x.png"}
        },
        "users": {
            "42": ["tier20","owner","founder","tier10","tier5"],
            "100": ["founder","unknown","founder","tier20"],
            "200": [],
            "300": "owner"
        }
    })json");
    ASSERT_TRUE(badges);
    ASSERT_EQ(badges->size(), 2U);
    ASSERT_TRUE(badges->contains("42"));
    ASSERT_TRUE(badges->contains("100"));
    const auto &owner = badges->at("42");
    ASSERT_EQ(owner.size(), 5U);
    EXPECT_EQ(owner[0]->id.string, "tier20");
    EXPECT_EQ(owner[1]->id.string, "owner");
    EXPECT_EQ(owner[2]->id.string, "founder");
    EXPECT_EQ(owner[3]->id.string, "tier10");
    EXPECT_EQ(owner[4]->id.string, "tier5");
    const auto &supporter = badges->at("100");
    ASSERT_EQ(supporter.size(), 2U);
    EXPECT_EQ(supporter[0]->id.string, "founder");
    EXPECT_EQ(supporter[1]->id.string, "tier20");
}

TEST(SatorinoBadges, ExpandedRosterShowsAllSevenSelectedBadgeTypes)
{
    const QJsonArray selected{"owner",    "dev",       "tester",   "founder",
                              "tier5-v4", "tier10-v2", "tier20-v3"};
    QJsonObject definitions;
    for (const auto &value : selected)
    {
        const auto id = value.toString();
        definitions.insert(
            id, QJsonObject{{"name", id},
                            {"image1x", "https://satorino.satos.cc/badges/" +
                                            id + "-1x.png"}});
    }
    const QJsonObject root{{"version", 2},
                           {"badges", definitions},
                           {"users", QJsonObject{{"42", selected}}}};
    const auto badges = satorino::parseBadgeFeed(QJsonDocument(root).toJson());
    ASSERT_TRUE(badges);
    ASSERT_TRUE(badges->contains("42"));
    const auto &owner = badges->at("42");
    ASSERT_EQ(owner.size(), 7U);
    for (qsizetype i = 0; i < selected.size(); ++i)
    {
        EXPECT_EQ(owner[static_cast<size_t>(i)]->id.string,
                  selected[i].toString());
    }
}
