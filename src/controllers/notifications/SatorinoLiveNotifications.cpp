// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/notifications/SatorinoLiveNotifications.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/Settings.hpp"
#include "singletons/StreamerMode.hpp"
#include "singletons/Toasts.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include <utility>

namespace chatterino::satorino {

LiveNotifications::LiveNotifications()
{
    this->clock_.start();
    this->timer_.setSingleShot(true);
    QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->pump();
    });
}

void LiveNotifications::setWatched(const QStringList &logins)
{
    this->watched_.clear();
    for (const auto &name : logins)
    {
        const auto login = normalizedLogin(name);
        if (!login.isEmpty() && !this->watched_.contains(login))
        {
            this->watched_.push_back(login);
        }
    }
    this->tracker_.retain(this->watched_);
}

void LiveNotifications::observe(const QString &login,
                                const std::optional<HelixStream> &stream,
                                bool initial)
{
    assertInGuiThread();
    const auto name = normalizedLogin(login);
    if (!this->watched_.contains(name) ||
        !this->tracker_.observe(name, stream.has_value(),
                                stream ? stream->id : QString{},
                                this->clock_.elapsed(), initial))
    {
        return;
    }
    // Honor the existing streamer-mode suppression for both destinations.
    if (getApp()->getStreamerMode()->isEnabled() &&
        getSettings()->streamerModeSuppressLiveNotifications)
    {
        return;
    }
    if (getSettings()->satorinoLiveDesktop.getValue())
    {
        getApp()->getToasts()->sendChannelNotification(name, stream->title);
    }
    if (getSettings()->satorinoLiveDiscordEnabled.getValue())
    {
        Pending item{getSettings()->satorinoLiveDiscordUrl.getValue().trimmed(),
                     liveWebhookMessage(name, stream->userName, stream->title,
                                        stream->gameName),
                     0,
                     false,
                     {},
                     name};
        this->enqueue(std::move(item));
    }
}

void LiveNotifications::test(std::function<void(QString)> result)
{
    // Explicit user action; may test without enabling automatic go-live delivery.
    this->enqueue({getSettings()->satorinoLiveDiscordUrl.getValue().trimmed(),
                   {{"content", "satorino webhook test. Notifications work "
                                "only while satorino is running."},
                    {"allowed_mentions", QJsonObject{{"parse", QJsonArray{}}}}},
                   0,
                   true,
                   std::move(result),
                   {}});
}

void LiveNotifications::enqueue(Pending item)
{
    assertInGuiThread();
    if (!discordWebhookValid(item.endpoint) || this->queue_.size() >= 16)
    {
        const auto message =
            !discordWebhookValid(item.endpoint)
                ? QString("Invalid Discord webhook URL. Use an HTTPS Discord "
                          "webhook endpoint.")
                : QString("Discord notification queue is full.");
        if (item.result)
        {
            item.result(message);
        }
        qCWarning(chatterinoNotification)
            << "Discord notification rejected (invalid endpoint or full queue)";
        return;
    }
    this->queue_.push_back(std::move(item));
    if (!this->active_ && !this->timer_.isActive())
    {
        this->pump();
    }
}

void LiveNotifications::pump()
{
    if (this->active_ || this->queue_.empty())
    {
        return;
    }
    auto &item = this->queue_.front();
    if ((!item.test && (!getSettings()->satorinoLiveDiscordEnabled.getValue() ||
                        !this->watched_.contains(item.login))) ||
        item.endpoint !=
            getSettings()->satorinoLiveDiscordUrl.getValue().trimmed())
    {
        this->finish("Discord notification cancelled after settings changed.");
        return;
    }
    if (!discordWebhookValid(item.endpoint))
    {
        this->finish("Invalid Discord webhook URL.");
        return;
    }
    this->active_ = true;
    NetworkRequest(QUrl(item.endpoint), NetworkRequestType::Post)
        .sensitive()
        .followRedirects(false)
        .timeout(15'000)
        .caller(this)
        .json(item.body)
        .onSuccess([this](const NetworkResult &result) {
            this->completed(result);
        })
        .onError([this](const NetworkResult &result) {
            this->completed(result);
        })
        .execute();
}

void LiveNotifications::completed(const NetworkResult &result)
{
    this->active_ = false;
    if (this->queue_.empty())
    {
        return;
    }
    const int status = result.status().value_or(0);
    if (status >= 200 && status < 300)
    {
        this->finish("Discord message sent.");
        return;
    }
    auto retryAfter = result.retryAfter();
    if (status == 429 && retryAfter.isEmpty())
    {
        // Parse only retry metadata; never display/log a response body.
        const auto json = QJsonDocument::fromJson(result.getData()).object();
        if (json.value("retry_after").isDouble())
        {
            retryAfter =
                QByteArray::number(json.value("retry_after").toDouble());
        }
    }
    auto &item = this->queue_.front();
    const auto delay = webhookRetryDelay(status, item.retries, retryAfter,
                                         QDateTime::currentDateTimeUtc());
    if (delay)
    {
        ++item.retries;
        this->timer_.start(*delay);
        qCWarning(chatterinoNotification)
            << "Discord notification retry scheduled; HTTP status" << status;
        return;
    }
    qCWarning(chatterinoNotification)
        << "Discord notification failed; HTTP status" << status;
    this->finish(
        status == 0
            ? QString("Discord delivery failed (network error or timeout).")
            : "Discord delivery failed (HTTP status " +
                  QString::number(status) + ").");
}

void LiveNotifications::finish(const QString &message)
{
    auto result = std::move(this->queue_.front().result);
    this->queue_.pop_front();
    this->active_ = false;
    if (result)
    {
        result(message);
    }
    if (!this->queue_.empty())
    {
        this->timer_.start(500);
    }
}

}  // namespace chatterino::satorino
