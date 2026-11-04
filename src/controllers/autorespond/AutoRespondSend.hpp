// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <optional>

namespace chatterino {
class Channel;
}
namespace chatterino::satorino {
// Normal Channel::sendMessage, with a scoped final-payload guard. No raw sends,
// command expansion, separate API client, or alternate rate limiter path.
struct AutoRespondSubmission {
    QString prepared;
    std::optional<bool> accepted;
};
AutoRespondSubmission sendAutoRespond(Channel &channel, const QString &message);
bool isAutoRespondSend(const Channel *channel = nullptr);
QString limitAutoRespondSend(QString prepared, const Channel *channel);
void recordAutoRespondSend(const Channel *channel, bool accepted,
                           const QString &prepared = {});
}  // namespace chatterino::satorino
