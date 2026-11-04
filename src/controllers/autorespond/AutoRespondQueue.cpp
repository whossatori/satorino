// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondQueue.hpp"
#include <algorithm>
#include <utility>

namespace chatterino::satorino {
void AutoRespondQueue::push(AutoRespondQueuedSend item)
{
    this->items_.push_back(std::move(item));
}
void AutoRespondQueue::startHead(qint64 now)
{
    if (!this->empty() && !this->items_.front().startedAt)
        this->items_.front().startedAt = now;
}
bool AutoRespondQueue::expired(qint64 now) const
{
    return !this->empty() && this->front().startedAt &&
           now - *this->front().startedAt >= 120000;
}
qint64 AutoRespondQueue::wait(qint64 now, bool ordinaryTwitch,
                              qint64 providerWait) const
{
    qint64 delay = std::max(qint64(0), this->nextAttempt_ - now);
    if (this->lastDeparture_)
        delay = std::max(delay, *this->lastDeparture_ +
                                    (ordinaryTwitch ? 1100 : 1000) - now);
    return std::max(delay, providerWait);
}
void AutoRespondQueue::refused(qint64 now)
{
    this->nextAttempt_ = now + 1100;
}
void AutoRespondQueue::finish(quint64 token, qint64 now, bool sent)
{
    if (sent)
        this->lastDeparture_ = now;
    if (!this->empty() && this->front().token == token)
        this->items_.pop_front();
    this->nextAttempt_ = 0;
}
std::deque<AutoRespondQueuedSend> AutoRespondQueue::clear()
{
    this->nextAttempt_ = 0;
    return std::exchange(this->items_, {});  // pacing survives master off/on
}
qint64 autoRespondRateWait(std::span<const qint64> history, int maximum,
                           qint64 minimumOffset, qint64 window, qint64 now)
{
    if (history.empty())
        return 0;
    qint64 wait = std::max(qint64(0), history.back() + minimumOffset - now);
    const auto active = std::ranges::find_if(history, [&](qint64 at) {
        return at + window >= now;
    });
    const auto count = history.end() - active;
    if (maximum > 0 && count >= maximum)
        wait = std::max(wait, *(active + (count - maximum)) + window + 1 - now);
    return wait;
}
}  // namespace chatterino::satorino
