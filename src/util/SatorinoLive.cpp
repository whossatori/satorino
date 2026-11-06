// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoLive.hpp"

#include <QJsonArray>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace chatterino::satorino {

QStringList normalizedLines(const QString &text)
{
    QStringList result;
    for (const auto &line : text.split('\n'))
    {
        const auto value = line.trimmed()
                               .normalized(QString::NormalizationForm_C)
                               .toCaseFolded();
        if (!value.isEmpty() && !result.contains(value))
        {
            result.push_back(value);
        }
    }
    return result;
}

bool gameMatches(const QString &list, const QString &name, const QString &id)
{
    const auto entries = normalizedLines(list);
    const auto canonical = [](const QString &value) {
        return value.trimmed()
            .normalized(QString::NormalizationForm_C)
            .toCaseFolded();
    };
    return (!name.trimmed().isEmpty() && entries.contains(canonical(name))) ||
           (!id.trimmed().isEmpty() && entries.contains(canonical(id)));
}

LiveIndicator liveIndicator(bool live, bool game)
{
    return !live  ? LiveIndicator::None
           : game ? LiveIndicator::Game
                  : LiveIndicator::Dot;
}

QString normalizedLogin(const QString &login)
{
    const auto value = login.trimmed().toLower();
    return !value.isEmpty() && value.size() <= 25 &&
                   std::all_of(value.begin(), value.end(),
                               [](QChar c) {
                                   return (c >= 'a' && c <= 'z') ||
                                          (c >= '0' && c <= '9') || c == '_';
                               })
               ? value
               : QString{};
}

bool discordWebhookValid(const QString &value)
{
    if (value.size() > 4096)
        return false;
    const QUrl url(value.trimmed(), QUrl::StrictMode);
    static const QSet<QString> hosts{
        "discord.com",        "discordapp.com",     "ptb.discord.com",
        "canary.discord.com", "ptb.discordapp.com", "canary.discordapp.com"};
    if (!url.isValid() || url.scheme() != "https" ||
        !hosts.contains(url.host().toLower()) || !url.userInfo().isEmpty() ||
        (url.port() != -1 && url.port() != 443) || url.hasFragment())
    {
        return false;
    }
    const auto path = url.path();
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    return path.startsWith("/api/webhooks/") && parts.size() >= 4 &&
           !parts.contains("..") && !parts.contains(".") &&
           !path.contains('\\');
}

QJsonObject liveWebhookMessage(const QString &login, const QString &displayName,
                               const QString &title, const QString &game)
{
    const auto name = normalizedLogin(login);
    if (name.isEmpty())
    {
        return {};
    }
    const QString link = " https://twitch.tv/" + name;
    QString body = (displayName.isEmpty() ? name : displayName.simplified()) +
                   " is live: " + title.simplified() + " — " +
                   game.simplified();
    const auto limit = 2000 - link.size();
    if (body.size() > limit)
    {
        body = body.left(limit - 1);
        if (!body.isEmpty() && body.at(body.size() - 1).isHighSurrogate())
        {
            body.chop(1);
        }
        body += QChar(0x2026);
    }
    return {{"content", body + link},
            {"allowed_mentions", QJsonObject{{"parse", QJsonArray{}}}}};
}

std::optional<int> webhookRetryDelay(int status, int retries,
                                     const QByteArray &retryAfter,
                                     const QDateTime &now)
{
    if (retries < 0 || retries >= 2 ||
        (status != 429 && (status < 500 || status > 599)))
    {
        return std::nullopt;
    }
    double seconds = 1 << retries;
    if (!retryAfter.isEmpty())
    {
        bool numeric = false;
        const auto number = retryAfter.toDouble(&numeric);
        if (numeric)
        {
            seconds = number;
        }
        else
        {
            const auto when = QDateTime::fromString(
                QString::fromLatin1(retryAfter), Qt::RFC2822Date);
            if (!when.isValid())
            {
                return std::nullopt;
            }
            seconds = std::max<qint64>(0, now.msecsTo(when)) / 1000.0;
        }
    }
    // Do not retry earlier than requested; abandon very long waits instead.
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 300)
    {
        return std::nullopt;
    }
    return std::max(1000, int(std::ceil(seconds * 1000)));
}

bool GoLiveTracker::observe(const QString &login, bool live,
                            const QString &streamId, qint64 nowMs, bool initial)
{
    const auto name = normalizedLogin(login);
    if (name.isEmpty())
    {
        return false;
    }
    auto it = this->states_.find(name);
    if (it == this->states_.end())
    {
        if (this->states_.size() >= MAX_STREAMERS)
        {
            const auto oldest = std::min_element(
                this->states_.begin(), this->states_.end(),
                [](const auto &a, const auto &b) {
                    return a.second.observedAt < b.second.observedAt;
                });
            this->states_.erase(oldest);
        }
        this->states_[name] = {live, streamId, {}, nowMs};
        return false;
    }
    auto &state = it->second;
    const bool notify = !initial && !state.live && live &&
                        !streamId.isEmpty() && streamId != state.streamId &&
                        (!state.sentAt || nowMs - *state.sentAt >= COOLDOWN_MS);
    state.live = live;
    state.observedAt = nowMs;
    if (live && !streamId.isEmpty())
    {
        state.streamId = streamId;
    }
    if (notify)
    {
        state.sentAt = nowMs;
    }
    return notify;
}

void GoLiveTracker::retain(const QStringList &logins)
{
    QSet<QString> keep;
    for (const auto &name : logins)
    {
        keep.insert(normalizedLogin(name));
    }
    std::erase_if(this->states_, [&](const auto &entry) {
        return !keep.contains(entry.first);
    });
}

}  // namespace chatterino::satorino
