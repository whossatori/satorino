// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoLive.hpp"

#include "common/network/NetworkPrivate.hpp"
#include "common/network/NetworkResult.hpp"
#include "Test.hpp"

#include <QJsonArray>
#include <QUrl>

using namespace chatterino;
using namespace chatterino::satorino;

namespace {
// Synthetic pieces only, never a real configured webhook or credential.
QString endpoint(const QString &host, const QString &scheme = "https",
                 const QString &path = "/api/webhooks/1/synthetic-test-token")
{
    QUrl value;
    value.setScheme(scheme);
    value.setHost(host);
    value.setPath(path);
    return value.toString();
}
}  // namespace

TEST(SatorinoLive, GamesAreExactTrimmedAndCaseInsensitive)
{
    EXPECT_TRUE(
        gameMatches("  Just Chatting \n\n  Minecraft \r\n", " minecraft "));
    EXPECT_FALSE(gameMatches("Minecraft", "Minecraft Dungeons"));
    EXPECT_FALSE(gameMatches("Minecraft Dungeons", "Minecraft"));
    EXPECT_FALSE(gameMatches(" \n\t", "Minecraft"));
    EXPECT_FALSE(gameMatches("Minecraft", ""));
}

TEST(SatorinoLive, GamesMatchIdsAndUnicodeCanonicalNames)
{
    EXPECT_TRUE(gameMatches("509658", "Just Chatting", "509658"));
    EXPECT_TRUE(gameMatches(QString::fromUtf8("POKÉMON"),
                            QString::fromUtf8("Poke\xCC\x81mon")));
    EXPECT_TRUE(
        gameMatches(QString::fromUtf8("日本語"), QString::fromUtf8("日本語")));
    EXPECT_FALSE(
        gameMatches(QString::fromUtf8("日本語"), QString::fromUtf8("日本")));
}

TEST(SatorinoLive, IndicatorPriorityIsOfflineThenTrainGameDot)
{
    EXPECT_EQ(liveIndicator(false, true, true), LiveIndicator::None);
    EXPECT_EQ(liveIndicator(true, true, true), LiveIndicator::Train);
    EXPECT_EQ(liveIndicator(true, true), LiveIndicator::Game);
    EXPECT_EQ(liveIndicator(true, false), LiveIndicator::Dot);
}

TEST(SatorinoLive, FirstObservationIsNeverAnEvent)
{
    GoLiveTracker tracker;
    EXPECT_FALSE(tracker.observe("already_live", true, "stream1", 0));
    EXPECT_FALSE(tracker.observe("offline", false, {}, 0));
    EXPECT_FALSE(tracker.observe("already_live", true, "stream1", 1000));
}

TEST(SatorinoLive, OfflineToLiveNotifiesOnceAcrossSources)
{
    GoLiveTracker tracker;
    EXPECT_FALSE(tracker.observe("test_streamer", false, {}, 0));
    EXPECT_TRUE(tracker.observe(" TEST_STREAMER ", true, "stream1", 1000));
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream1", 2000));
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream2", 3000));
}

TEST(SatorinoLive, ReconnectAndSameStreamNeverNotify)
{
    GoLiveTracker tracker;
    tracker.observe("test_streamer", false, {}, 0);
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream1", 1, true));
    tracker.observe("test_streamer", false, {}, 2);
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream1",
                                 GoLiveTracker::COOLDOWN_MS + 3));
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream1",
                                 GoLiveTracker::COOLDOWN_MS + 4));
}

TEST(SatorinoLive, CooldownSuppressesFlappingAndAllowsLaterNewStream)
{
    GoLiveTracker tracker;
    tracker.observe("test_streamer", false, {}, 0);
    EXPECT_TRUE(tracker.observe("test_streamer", true, "stream1", 1000));
    tracker.observe("test_streamer", false, {}, 2000);
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream2", 3000));
    tracker.observe("test_streamer", false, {}, 4000);
    EXPECT_TRUE(tracker.observe("test_streamer", true, "stream3",
                                GoLiveTracker::COOLDOWN_MS + 1000));
}

TEST(SatorinoLive, MissingStreamIdAndInvalidLoginCannotNotify)
{
    GoLiveTracker tracker;
    tracker.observe("test_streamer", false, {}, 0);
    EXPECT_FALSE(tracker.observe("test_streamer", true, {}, 1000));
    EXPECT_FALSE(tracker.observe("bad/login", true, "id", 1000));
    EXPECT_FALSE(tracker.observe("", true, "id", 1000));
}

TEST(SatorinoLive, RemovingWatcherResetsItsBaseline)
{
    GoLiveTracker tracker;
    tracker.observe("test_streamer", false, {}, 0);
    tracker.retain({});
    EXPECT_FALSE(tracker.observe("test_streamer", true, "stream1", 1000));
}

TEST(SatorinoLive, EvictionIsBoundedAndNewObservationsStaySilent)
{
    GoLiveTracker tracker;
    for (int i = 0; i < GoLiveTracker::MAX_STREAMERS + 1; ++i)
    {
        EXPECT_FALSE(
            tracker.observe("streamer" + QString::number(i), false, {}, i));
    }
    EXPECT_FALSE(tracker.observe("streamer0", true, "stream1", 1000));
}

TEST(SatorinoLive, WebhookAcceptsOnlyApprovedHttpsHostsAndPaths)
{
    for (const auto &host :
         {"discord.com", "discordapp.com", "ptb.discord.com",
          "canary.discord.com", "ptb.discordapp.com", "canary.discordapp.com"})
    {
        EXPECT_TRUE(discordWebhookValid(endpoint(host)));
    }
    for (const auto &host :
         {"discord.com.example.invalid", "notdiscord.com",
          "discord.example.invalid", "localhost", "127.0.0.1"})
    {
        EXPECT_FALSE(discordWebhookValid(endpoint(host)));
    }
    EXPECT_FALSE(discordWebhookValid(endpoint("discord.com", "http")));
    EXPECT_FALSE(discordWebhookValid(endpoint("discord.com", "https", "/")));
    EXPECT_FALSE(discordWebhookValid(
        endpoint("discord.com", "https", "/api/webhooks/")));
    EXPECT_FALSE(discordWebhookValid(
        endpoint("discord.com", "https", "/api/webhooks/1")));
    EXPECT_FALSE(discordWebhookValid(
        endpoint("discord.com", "https", "/api/webhooks/../other/token")));
    EXPECT_FALSE(discordWebhookValid("garbage"));
    EXPECT_FALSE(discordWebhookValid(endpoint(
        "discord.com", "https", "/api/webhooks/1/" + QString(5000, 'x'))));
}

TEST(SatorinoLive, WebhookRejectsCredentialsNonstandardPortsAndFragments)
{
    auto url = QUrl(endpoint("discord.com"));
    url.setUserName("other");
    EXPECT_FALSE(discordWebhookValid(url.toString()));
    url.setUserName({});
    url.setPort(8443);
    EXPECT_FALSE(discordWebhookValid(url.toString()));
    url.setPort(443);
    EXPECT_TRUE(discordWebhookValid(url.toString()));
    url.setFragment("fragment");
    EXPECT_FALSE(discordWebhookValid(url.toString()));
}

TEST(SatorinoLive, FormattingPreservesLinkAndDisablesAllMentions)
{
    const auto result = liveWebhookMessage("test_streamer", "Display",
                                           "@everyone Title", "Game");
    EXPECT_EQ(result.value("content").toString(),
              "Display is live: @everyone Title — Game "
              "https://twitch.tv/test_streamer");
    EXPECT_TRUE(result.value("allowed_mentions")
                    .toObject()
                    .value("parse")
                    .toArray()
                    .isEmpty());
    EXPECT_TRUE(liveWebhookMessage("bad/login", "", "", "").isEmpty());
}

TEST(SatorinoLive, LongTitlesFitDiscordLimitWithoutCuttingSurrogates)
{
    const auto result = liveWebhookMessage("test_streamer", QString(3000, 'x'),
                                           QString(4000, 'y'), "game");
    const auto content = result.value("content").toString();
    EXPECT_LE(content.size(), 2000);
    EXPECT_TRUE(content.endsWith("https://twitch.tv/test_streamer"));
    QString unicode;
    const auto emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    for (int i = 0; i < 2000; ++i)
        unicode += emoji;
    const auto text =
        liveWebhookMessage("test_streamer", "Display", unicode, "Game")
            .value("content")
            .toString();
    EXPECT_LE(text.size(), 2000);
    EXPECT_EQ(QString::fromUtf8(text.toUtf8()), text);
    EXPECT_TRUE(result.value("allowed_mentions")
                    .toObject()
                    .value("parse")
                    .toArray()
                    .isEmpty());
}

TEST(SatorinoLive, RetryPolicyIsBoundedAndHonorsRetryAfter)
{
    const auto now = QDateTime::fromString("2026-11-04T12:00:00Z", Qt::ISODate);
    EXPECT_EQ(webhookRetryDelay(429, 0, "2.5", now), 2500);
    EXPECT_EQ(webhookRetryDelay(503, 1, {}, now), 2000);
    EXPECT_EQ(
        webhookRetryDelay(
            429, 0, now.addSecs(15).toString(Qt::RFC2822Date).toLatin1(), now),
        15000);
    EXPECT_FALSE(webhookRetryDelay(429, 2, "1", now));
    EXPECT_FALSE(webhookRetryDelay(400, 0, {}, now));
    EXPECT_FALSE(webhookRetryDelay(0, 0, {}, now));
    EXPECT_FALSE(webhookRetryDelay(429, 0, "3600", now));
    EXPECT_FALSE(webhookRetryDelay(429, 0, "garbage", now));
}

TEST(SatorinoLive, PrivateLogUrlAndRetryMetadata)
{
    NetworkData data;
    data.request.setUrl(QUrl(endpoint("discord.com")));
    data.sensitive = true;
    EXPECT_EQ(data.logUrl().scheme(), "redacted");
    EXPECT_FALSE(data.logUrl().toString().contains("synthetic-test-token"));
    const NetworkResult result(QNetworkReply::NoError, 204, {}, {}, "2");
    EXPECT_EQ(result.retryAfter(), "2");
}
