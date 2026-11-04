// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondSend.hpp"

#include "common/Channel.hpp"
#include "controllers/autorespond/AutoRespondData.hpp"
#include "debug/AssertInGuiThread.hpp"

#include <QCoreApplication>

#include <atomic>
#include <memory>
#include <utility>

namespace chatterino::satorino {
namespace {
struct SendState {
    const Channel *target = nullptr;
    AutoRespondSubmission result;
};
thread_local std::shared_ptr<SendState> activeSend;
std::atomic_bool sending{false};

class SendScope
{
public:
    explicit SendScope(std::shared_ptr<SendState> state)
        : previous_(std::exchange(activeSend, std::move(state)))
    {
        sending.store(true);
    }
    ~SendScope()
    {
        activeSend = std::move(this->previous_);
        sending.store(activeSend != nullptr);
    }

private:
    std::shared_ptr<SendState> previous_;
};
}  // namespace

AutoRespondSubmission sendAutoRespond(Channel &channel, const QString &message)
{
    if (!QCoreApplication::instance() || !isGuiThread() ||
        isAutoRespondSend() || QCoreApplication::closingDown())
        return {};
    // Owned state never points at a return value or a stack local. No temporary
    // observer is attached to the channel signal, even for a synchronous send.
    const auto state = std::make_shared<SendState>();
    state->target = &channel;
    const SendScope scope(state);
    channel.sendMessage(message);
    return state->result;
}

bool isAutoRespondSend(const Channel *channel)
{
    return channel ? activeSend && activeSend->target == channel
                   : sending.load();
}

QString limitAutoRespondSend(QString prepared, const Channel *channel)
{
    if (!isAutoRespondSend(channel) || !activeSend)
        return prepared;
    activeSend->result.prepared = sanitizeAutoRespond(prepared);
    return activeSend->result.prepared;
}

void recordAutoRespondSend(const Channel *channel, bool accepted)
{
    if (isAutoRespondSend(channel) && activeSend)
        activeSend->result.accepted = accepted;
}
}  // namespace chatterino::satorino
