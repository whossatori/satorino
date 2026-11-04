// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondQueue.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/kick/KickChannel.hpp"
#include "singletons/Settings.hpp"
#include "debug/AssertInGuiThread.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace chatterino {
std::chrono::milliseconds TwitchIrcServer::autoRespondSendWait(
    const std::shared_ptr<TwitchChannel> &channel)
{
    assertInGuiThread();
    if (!channel)
        return std::chrono::milliseconds(120000);
    if (!getSettings()->shouldSendHelixChat())
    {
        std::lock_guard<std::mutex> connectionLock(this->connectionMutex_);
        if (!this->writeConnection_ || !this->writeConnection_->isConnected())
            return std::chrono::milliseconds(1000);
    }
    // Read the SAME histories used by prepareToSend. No send, logging, signal or
    // system-message insertion happens under this mutex during this query.
    std::lock_guard<std::mutex> lock(this->lastMessageMutex_);
    const bool high = channel->hasHighRateLimit();
    auto history = high ? this->lastMessageMod_ : this->lastMessagePleb_;
    std::vector<qint64> times;
    while (!history.empty())
    {
        times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(
                            history.front().time_since_epoch())
                            .count());
        history.pop();
    }
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    const int maximum =
        getSettings()->useBotLimitsMessage ? 7499 : (high ? 99 : 19);
    // A small rounding margin avoids calling the strict nanosecond limiter early.
    return std::chrono::milliseconds(satorino::autoRespondRateWait(
        times, maximum, high ? 102 : 1102, 32002, now));
}
std::chrono::milliseconds KickChannel::autoRespondSendWait() const
{
    assertInGuiThread();
    auto history = this->lastMessageTimestamps_;
    std::vector<qint64> times;
    while (!history.empty())
    {
        times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(
                            history.front().time_since_epoch())
                            .count());
        history.pop();
    }
    const auto now = std::chrono::steady_clock::now();
    auto wait = satorino::autoRespondRateWait(
        times, static_cast<int>(this->maxBurstMessages()),
        this->minMessageOffset().count() + 2, 5002,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
            .count());
    if (this->sendWaitEnd_ && *this->sendWaitEnd_ > now)
        wait = std::max<qint64>(
            wait, std::chrono::duration_cast<std::chrono::milliseconds>(
                      *this->sendWaitEnd_ - now)
                          .count() +
                      1);
    return std::chrono::milliseconds(wait);
}
}  // namespace chatterino
