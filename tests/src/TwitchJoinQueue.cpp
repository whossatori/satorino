// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/TwitchJoinQueue.hpp"

#include <QEventLoop>
#include <QStringList>
#include <QTimer>
#include <gtest/gtest.h>

namespace chatterino {
namespace {

void waitForTimers(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

TEST(TwitchJoinQueue, RetriesOnlyUnconfirmedChannelsAndOnlyOnce)
{
    QStringList sent;
    TwitchJoinQueue queue(2, 25, 10, [&](const QString &channel, bool retry) {
        sent.append(retry ? channel + " retry" : channel);
    });

    queue.request("confirmed");
    queue.request("silent");
    queue.request("silent");
    queue.confirm("confirmed");
    EXPECT_EQ(sent.size(), 2);

    waitForTimers(100);
    EXPECT_TRUE((sent == QStringList{"confirmed", "silent", "silent retry"}));
}

TEST(TwitchJoinQueue, LateConfirmationCancelsQueuedRetry)
{
    QStringList sent;
    TwitchJoinQueue queue(1, 160, 5, [&](const QString &channel, bool retry) {
        sent.append(retry ? channel + " retry" : channel);
    });

    queue.request("confirmed");
    waitForTimers(25);
    queue.confirm("confirmed");
    EXPECT_EQ(sent.size(), 1);

    waitForTimers(200);
    EXPECT_TRUE((sent == QStringList{"confirmed"}));
}

TEST(TwitchJoinQueue, IgnoresConfirmationBeforeFirstSendAndCancelsClosedTabs)
{
    QStringList sent;
    TwitchJoinQueue queue(1, 30, 100, [&](const QString &channel, bool retry) {
        sent.append(retry ? channel + " retry" : channel);
    });

    queue.request("first");
    queue.confirm("first");
    queue.request("closed");
    queue.cancel("closed");
    queue.request("next");
    queue.confirm("next");

    waitForTimers(75);
    EXPECT_TRUE((sent == QStringList{"first", "next"}));
}

TEST(TwitchJoinQueue, ResetDropsStaleWorkAndPreservesCooldown)
{
    QStringList sent;
    TwitchJoinQueue queue(1, 30, 10, [&](const QString &channel, bool retry) {
        sent.append(retry ? channel + " retry" : channel);
    });

    queue.request("previous");
    queue.request("queued");
    queue.reset();
    queue.request("next");
    EXPECT_EQ(sent.size(), 1);

    waitForTimers(100);
    EXPECT_TRUE((sent == QStringList{"previous", "next", "next retry"}));
}

}  // namespace
}  // namespace chatterino
