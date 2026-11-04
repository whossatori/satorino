// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/autorespond/AutoRespondData.hpp"
#include "messages/Message.hpp"

#include <boost/signals2/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <deque>
#include <memory>

namespace chatterino {
class Channel;
enum class MessageContext;
}  // namespace chatterino
namespace chatterino::satorino {

class AutoRespondController : public QObject
{
public:
    explicit AutoRespondController(QObject *parent);
    void receive(const std::shared_ptr<Channel> &channel,
                 const MessagePtr &message);

private:
    void tick();
    void rebuildTriggers();
    void submit(const std::shared_ptr<Channel> &channel, const QString &text,
                const QString &identity, bool sound = false,
                const QString &customSound = {});
    void resetSchedule();

    QTimer timer_;
    QElapsedTimer clock_;
    QDateTime previous_;
    qint64 previousSteady_ = 0;
    quint64 revision_ = 0;
    AutoRespondLimits limits_;
    std::vector<std::pair<AutoRespondTrigger, QRegularExpression>> triggers_;
    boost::signals2::scoped_connection twitchConnection_;
    std::map<QString, QDate> consumed_;
    struct Seen {
        QString key;
        qint64 at;
    };
    std::deque<Seen> seen_;
    pajlada::Signals::SignalHolder connections_;
};

AutoRespondController &autoRespond();
// Single common insertion hook. History insertion/merge paths never call it;
// reposts, system messages, history flags and unsupported channels are rejected.
void observeAutoRespond(Channel &channel, const MessagePtr &message,
                        MessageContext context,
                        std::optional<MessageFlags> overridingFlags);

}  // namespace chatterino::satorino
