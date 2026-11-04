// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondSend.hpp"

#include "common/Channel.hpp"
#include "controllers/autorespond/AutoRespondData.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QScopedValueRollback>

#include <utility>

namespace chatterino::satorino {
namespace {
thread_local AutoRespondSubmission *submission = nullptr;
}

AutoRespondSubmission sendAutoRespond(Channel &channel, const QString &message)
{
    assertInGuiThread();
    AutoRespondSubmission result;
    const QScopedValueRollback guard(submission, &result);
    pajlada::Signals::SignalHolder connections;
    if (channel.getType() == Channel::Type::Twitch)
    {
        // Registered after the server's normal handler. Capture its local rate
        // limiter result; remote API/IRC acceptance remains asynchronous.
        auto &twitch = static_cast<TwitchChannel &>(channel);
        connections.managedConnect(twitch.sendMessageSignal,
                                   [&](const QString &, bool &sent) {
                                       result.accepted = sent;
                                   });
    }
    channel.sendMessage(message);
    return result;
}

bool isAutoRespondSend()
{
    return submission != nullptr;
}

QString limitAutoRespondSend(QString prepared)
{
    if (!submission)
        return prepared;
    submission->prepared = sanitizeAutoRespond(prepared);
    return submission->prepared;
}
}  // namespace chatterino::satorino
