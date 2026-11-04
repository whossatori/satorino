// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoLive.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <deque>
#include <functional>

namespace chatterino {
struct HelixStream;
class NetworkResult;
}  // namespace chatterino

namespace chatterino::satorino {

class LiveNotifications : public QObject
{
public:
    LiveNotifications();
    void setWatched(const QStringList &logins);
    void observe(const QString &login, const std::optional<HelixStream> &stream,
                 bool initial);
    void test(std::function<void(QString)> result);

private:
    struct Pending {
        QString endpoint;
        QJsonObject body;
        int retries = 0;
        bool test = false;
        std::function<void(QString)> result;
        QString login;
    };
    void enqueue(Pending item);
    void pump();
    void completed(const NetworkResult &result);
    void finish(const QString &message);

    GoLiveTracker tracker_;
    QStringList watched_;
    QElapsedTimer clock_;
    QTimer timer_;
    std::deque<Pending> queue_;
    bool active_ = false;
};

}  // namespace chatterino::satorino
