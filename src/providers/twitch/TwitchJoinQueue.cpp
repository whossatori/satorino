// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/TwitchJoinQueue.hpp"

#include <QTimer>

#include <utility>

namespace chatterino {

TwitchJoinQueue::TwitchJoinQueue(
    int budget, int cooldown, int confirmationTimeout,
    std::function<void(const QString &, bool)> sendJoin)
    : confirmationTimeout_(confirmationTimeout)
    , sendJoin_(std::move(sendJoin))
    , bucket_(
          budget, cooldown,
          [this](const QString &channel) {
              this->send(channel);
          },
          nullptr)
{
}

void TwitchJoinQueue::request(const QString &channel)
{
    if (this->pending_.contains(channel))
    {
        return;
    }

    this->pending_.insert(channel, 0);
    this->bucket_.send(channel);
}

void TwitchJoinQueue::confirm(const QString &channel)
{
    if (this->pending_.value(channel) != 0)
    {
        this->cancel(channel);
    }
}

void TwitchJoinQueue::cancel(const QString &channel)
{
    this->pending_.remove(channel);
    this->bucket_.cancelPending(channel);
}

void TwitchJoinQueue::reset()
{
    this->pending_.clear();
    this->bucket_.clearPending();
}

void TwitchJoinQueue::send(const QString &channel)
{
    auto pending = this->pending_.find(channel);
    if (pending == this->pending_.end())
    {
        return;
    }

    if (pending.value() != 0)
    {
        this->pending_.erase(pending);
        this->sendJoin_(channel, true);
        return;
    }

    const auto attempt = ++this->nextAttempt_;
    pending.value() = attempt;
    this->sendJoin_(channel, false);

    QTimer::singleShot(this->confirmationTimeout_, this,
                       [this, channel, attempt] {
                           if (this->pending_.value(channel) == attempt)
                           {
                               this->bucket_.send(channel);
                           }
                       });
}

}  // namespace chatterino
