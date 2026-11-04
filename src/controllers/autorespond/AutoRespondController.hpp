// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include "controllers/autorespond/AutoRespondQueue.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "messages/Message.hpp"
#include <boost/signals2/connection.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace chatterino {
class IApplication;
}
namespace chatterino::satorino {
class AutoRespondController : public QObject
{
public:
    explicit AutoRespondController(IApplication &app);
    ~AutoRespondController() override;
    void start();
    void shutdown();
    AutoRespondSettings &settings()
    {
        return this->settings_;
    }
    void receive(const std::shared_ptr<Channel> &channel,
                 const MessagePtr &message);

private:
    void tick();
    void rebuildTriggers();
    void resetSchedule(bool invalidate = true);
    bool ruleCurrent(const AutoRespondRule &rule) const;
    void enqueue(const std::shared_ptr<Channel> &channel,
                 const AutoRespondRule &rule, const QString &text,
                 const QString &account,
                 const std::shared_ptr<AutoRespondBatch> &batch = {});
    void processQueue();
    void clearQueue();
    void finishBatch(const AutoRespondQueuedSend &item, bool sent);
    void postNote(std::weak_ptr<Channel> channel, QString text);
    IApplication &app_;
    AutoRespondSettings settings_;
    bool running_ = false, processing_ = false;
    QTimer timer_, queueTimer_;
    QElapsedTimer clock_;
    QDateTime previous_;
    qint64 previousSteady_ = 0;
    quint64 revision_ = 0, nextToken_ = 0, activeToken_ = 0;
    AutoRespondLimits limits_;
    AutoRespondQueue queue_;
    std::vector<std::pair<AutoRespondRule, QRegularExpression>> triggers_;
    std::map<QString, QDate> consumed_;
    struct Seen {
        QString key;
        qint64 at;
    };
    std::deque<Seen> seen_;
    boost::signals2::scoped_connection twitchConnection_;
    pajlada::Signals::SignalHolder connections_;
};
AutoRespondController *tryAutoRespond();
void observeAutoRespond(Channel &channel, const MessagePtr &message,
                        MessageContext context,
                        std::optional<MessageFlags> overridingFlags);
}  // namespace chatterino::satorino
