// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include "controllers/autorespond/AutoRespondRule.hpp"
#include <deque>
#include <optional>
#include <memory>
#include <span>

namespace chatterino {
class Channel;
}
namespace chatterino::satorino {
struct AutoRespondBatch {
    int total = 0, sent = 0, skipped = 0, remaining = 0;
    std::vector<std::weak_ptr<Channel>> targets;
};
struct AutoRespondQueuedSend {
    quint64 token = 0, session = 0;
    std::weak_ptr<Channel> channel;
    AutoRespondRule rule;
    QString text, account;
    std::shared_ptr<AutoRespondBatch> batch;
    std::optional<qint64> startedAt;
};
// Pure FIFO/pacing policy; all times are injectable monotonic milliseconds.
class AutoRespondQueue
{
public:
    void push(AutoRespondQueuedSend item);
    bool empty() const
    {
        return this->items_.empty();
    }
    const AutoRespondQueuedSend &front() const
    {
        return this->items_.front();
    }
    void startHead(qint64 now);
    bool expired(qint64 now) const;
    qint64 wait(qint64 now, bool ordinaryTwitch, qint64 providerWait = 0) const;
    void refused(qint64 now);
    void finish(quint64 token, qint64 now, bool sent);
    std::deque<AutoRespondQueuedSend> clear();

private:
    std::deque<AutoRespondQueuedSend> items_;
    std::optional<qint64> lastDeparture_;
    qint64 nextAttempt_ = 0;
};
qint64 autoRespondRateWait(std::span<const qint64> history, int maximum,
                           qint64 minimumOffset, qint64 window, qint64 now);
}  // namespace chatterino::satorino
