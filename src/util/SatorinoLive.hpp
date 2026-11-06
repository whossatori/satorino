// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>  // satorino: public retry metadata type
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>

#include <map>
#include <optional>

namespace chatterino::satorino {

// Shared presentation priority; producer availability is independent of live/game state.
enum class LiveIndicator { None, Dot, Game, Train };
QStringList normalizedLines(const QString &text);
bool gameMatches(const QString &list, const QString &name,
                 const QString &id = {});
LiveIndicator liveIndicator(bool live, bool game, bool train = false);
QString normalizedLogin(const QString &login);
bool discordWebhookValid(const QString &url);
QJsonObject liveWebhookMessage(const QString &login, const QString &displayName,
                               const QString &title, const QString &game);
std::optional<int> webhookRetryDelay(int status, int retries,
                                     const QByteArray &retryAfter,
                                     const QDateTime &now);

class GoLiveTracker
{
public:
    static constexpr qint64 COOLDOWN_MS = 10 * 60 * 1000;
    static constexpr int MAX_STREAMERS = 512;
    bool observe(const QString &login, bool live, const QString &streamId,
                 qint64 nowMs, bool initial = false);
    void retain(const QStringList &logins);

private:
    struct State {
        bool live = false;
        QString streamId;
        std::optional<qint64> sentAt;
        qint64 observedAt = 0;
    };
    std::map<QString, State> states_;
};

}  // namespace chatterino::satorino
