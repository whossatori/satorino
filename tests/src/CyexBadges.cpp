// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "providers/cyex/CyexBadges.hpp"

#include "messages/Image.hpp"
#include "Test.hpp"

#include <QDateTime>

#include <limits>

using namespace chatterino;

namespace {

const QByteArray CATALOG = R"json({
    "badges": [
        {
            "id": "founder", "title": "Cyex Founder",
            "description": "First <100> supporters",
            "images": {"1x": "https://api.cyex.app/badges/v1/img/founder/1x.png?v=1"}
        },
        {
            "id": "supporter", "title": "Cyex Supporter",
            "images": {"1x": "https://api.cyex.app/badges/v1/img/supporter/1x.png"}
        },
        {
            "id": "cyex-tester", "title": "Beta Tester",
            "images": {"1x": "https://api.cyex.app/badges/v1/img/cyex-tester/1x.png"}
        },
        {"id": "custom", "title": "Cyex Supporter+", "images": null}
    ]
})json";

}  // namespace

TEST(CyexBadges, ResolvesCatalogKindsByTwitchIdAndPreservesBadgeOrder)
{
    const auto catalog = cyex::parseCatalog(CATALOG);
    ASSERT_TRUE(catalog);
    const auto users = cyex::parseUsers(R"json({
        "users": {
            "9007199254740993": [
                {"id":"founder","number":37}, {"id":"supporter"},
                {"id":"unknown"}, {"id":"founder","number":99}, {"id":"cyex-tester"}
            ],
            "9007199254740994": [{"id":"founder","number":38}],
            "some_login": [{"id":"supporter"}]
        }
    })json",
                                        *catalog);
    ASSERT_TRUE(users);
    ASSERT_EQ(users->size(), 2U);
    ASSERT_TRUE(users->contains("9007199254740993"));
    ASSERT_TRUE(users->contains("9007199254740994"));
    const auto &badges = users->at("9007199254740993");
    ASSERT_EQ(badges.size(), 3U);
    EXPECT_EQ(badges[0]->tooltip.string,
              "Cyex Founder #37<br>First &lt;100&gt; supporters");
    EXPECT_EQ(badges[1]->name.string, "cyex:supporter");
    EXPECT_EQ(badges[2]->name.string, "cyex:cyex-tester");
    EXPECT_TRUE(
        users->at("9007199254740994").front()->tooltip.string.contains("#38"));
    EXPECT_EQ(badges[0]->images.getImage1(),
              catalog->at("founder").badge->images.getImage1());
}

TEST(CyexBadges, PrefersAnimatedVariantsAndFallsBackPerScale)
{
    const auto catalog = cyex::parseCatalog(R"json({
        "badges": [
            {
                "id": "founder", "title": "Cyex Founder", "animated": true,
                "animatedUrls": {
                    "1x": "https://api.cyex.app/badges/v1/img/founder/1x.webp?v=7eb01e38",
                    "2x": "https://api.cyex.app/badges/v1/img/founder/2x.webp?v=7eb01e38",
                    "4x": "https://other.example/4x.webp"
                },
                "images": {
                    "1x": "https://api.cyex.app/badges/v1/img/founder/1x.png",
                    "4x": "https://api.cyex.app/badges/v1/img/founder/4x.png"
                }
            },
            {
                "id": "supporter", "title": "Cyex Supporter", "animated": false,
                "animatedUrls": {"1x": "https://api.cyex.app/badges/v1/img/supporter/1x.webp"},
                "images": {"1x": "https://api.cyex.app/badges/v1/img/supporter/1x.png"}
            }
        ]
    })json");
    ASSERT_TRUE(catalog);
    ASSERT_EQ(catalog->size(), 2U);
    const auto &founder = catalog->at("founder").badge->images;
    EXPECT_EQ(founder.getImage1()->url().string,
              "https://api.cyex.app/badges/v1/img/founder/1x.webp?v=7eb01e38");
    EXPECT_EQ(founder.getImage2()->url().string,
              "https://api.cyex.app/badges/v1/img/founder/2x.webp?v=7eb01e38");
    EXPECT_EQ(founder.getImage3()->url().string,
              "https://api.cyex.app/badges/v1/img/founder/4x.png");
    EXPECT_EQ(catalog->at("supporter").badge->images.getImage1()->url().string,
              "https://api.cyex.app/badges/v1/img/supporter/1x.png");
}

TEST(CyexBadges, UsesPixelSizesAndScalesHigherResolutionToBadgeSize)
{
    const auto catalog = cyex::parseCatalog(R"json({
        "badges": [{
            "id": "tester", "title": "Tester", "animated": true,
            "animatedSizes": {
                "18": "https://api.cyex.app/badges/v1/img/tester/1x.webp?v=2",
                "128": "https://api.cyex.app/badges/v1/img/tester/128.webp?v=2",
                "512": "https://api.cyex.app/badges/v1/img/tester/512.webp?v=2"
            },
            "animatedUrls": {
                "1x": "https://api.cyex.app/badges/v1/img/tester/1x.webp?v=1"
            },
            "sizes": {"72": "https://api.cyex.app/badges/v1/img/tester/4x.png"}
        }, {
            "id": "supporter", "title": "Supporter",
            "images": {
                "1x": "https://api.cyex.app/badges/v1/img/supporter/1x.png",
                "large": "https://api.cyex.app/badges/v1/img/supporter/large.png"
            }
        }]
    })json");
    ASSERT_TRUE(catalog);
    const auto &tester = catalog->at("tester").badge->images;
    EXPECT_EQ(tester.getImage1()->url().string,
              "https://api.cyex.app/badges/v1/img/tester/1x.webp?v=2");
    EXPECT_EQ(tester.getImage3()->url().string,
              "https://api.cyex.app/badges/v1/img/tester/4x.png");
    EXPECT_EQ(tester.getImage4()->url().string,
              "https://api.cyex.app/badges/v1/img/tester/128.webp?v=2");
    EXPECT_EQ(tester.getImage4()->size(), QSizeF(18, 18));
    EXPECT_EQ(catalog->at("supporter").badge->images.getImage4()->url().string,
              "https://api.cyex.app/badges/v1/img/supporter/large.png");
}

TEST(CyexBadges, CustomUsesItsOwnContentAddressedImages)
{
    const auto catalog = cyex::parseCatalog(CATALOG);
    ASSERT_TRUE(catalog);
    const auto users = cyex::parseUsers(R"json({
        "users": {
            "123": [{"id":"custom","images":{
                "1x":"https://api.cyex.app/badges/v1/img/u/123/9f3a/1x.png",
                "2x":"https://api.cyex.app/badges/v1/img/u/123/9f3a/2x.png",
                "4x":"https://api.cyex.app/badges/v1/img/u/123/9f3a/4x.png"}}],
            "234": [{"id":"custom"}],
            "345": [{"id":"custom","images":{"1x":"https://other.example/1x.png"}}]
        }
    })json",
                                        *catalog);
    ASSERT_TRUE(users);
    ASSERT_EQ(users->size(), 1U);
    ASSERT_TRUE(users->contains("123"));
    const auto &badges = users->at("123");
    ASSERT_EQ(badges.size(), 1U);
    const auto &badge = badges.front();
    EXPECT_EQ(badge->name.string, "cyex:custom");
    EXPECT_EQ(badge->images.getImage1()->url().string,
              "https://api.cyex.app/badges/v1/img/u/123/9f3a/1x.png");
    EXPECT_EQ(badge->images.getImage2()->url().string,
              "https://api.cyex.app/badges/v1/img/u/123/9f3a/2x.png");
    EXPECT_EQ(badge->images.getImage3()->url().string,
              "https://api.cyex.app/badges/v1/img/u/123/9f3a/4x.png");
}

TEST(CyexBadges, DistinguishesAnEmptySnapshotFromAnInvalidResponse)
{
    const auto catalog = cyex::parseCatalog(CATALOG);
    ASSERT_TRUE(catalog);
    EXPECT_FALSE(cyex::parseCatalog(R"({"badges":{}})"));
    EXPECT_FALSE(cyex::parseUsers("invalid JSON", *catalog));
    EXPECT_FALSE(cyex::parseUsers(R"({"users":[]})", *catalog));
    const auto empty = cyex::parseUsers(R"({"users":{}})", *catalog);
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
}

TEST(CyexBadges, HonorsRetryAfterSecondsAndHttpDate)
{
    const auto now = QDateTime::fromString("2026-11-10T12:00:00Z", Qt::ISODate);
    ASSERT_TRUE(now.isValid());
    EXPECT_EQ(cyex::retryAfterDelayMs("17", now), 17000);
    EXPECT_EQ(cyex::retryAfterDelayMs("Tue, 10 Nov 2026 12:00:30 GMT", now),
              30000);
    EXPECT_EQ(cyex::retryAfterDelayMs("0", now), 1000);
    EXPECT_EQ(cyex::retryAfterDelayMs("invalid", now), 1800000);
    EXPECT_EQ(cyex::retryAfterDelayMs("999999999999999999999", now),
              std::numeric_limits<qint64>::max());
}
