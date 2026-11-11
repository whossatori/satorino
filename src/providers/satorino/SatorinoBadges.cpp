// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "providers/satorino/SatorinoBadges.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "messages/Image.hpp"
#include "singletons/Settings.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>

namespace chatterino {
namespace {

using namespace literals;

constexpr auto BADGE_FEED_URL = "https://satorino.satos.cc/api/badges";
constexpr int REFRESH_INTERVAL_MS = 60 * 1000;
constexpr int RETRY_INTERVAL_MS = 5 * REFRESH_INTERVAL_MS;
constexpr qsizetype MAX_PAYLOAD_BYTES = 8 * 1024 * 1024;
constexpr qsizetype MAX_BADGE_KINDS = 64;
constexpr qsizetype MAX_USERS = 100000;
constexpr QSize BADGE_SIZE(18, 18);

ImagePtr badgeImage(const QJsonObject &badge, const QString &key, int scale)
{
    const auto address = badge.value(key).toString();
    const QUrl url(address, QUrl::StrictMode);
    if (address.size() > 2048 || !url.isValid() || url.scheme() != u"https"_s ||
        url.host() != u"satorino.satos.cc"_s || url.port(443) != 443 ||
        !url.userInfo().isEmpty())
    {
        return Image::getEmpty();
    }

    // The full URL is the image cache key, including any artwork version.
    return Image::fromUrl(Url{address}, 1.0 / scale, BADGE_SIZE * scale);
}

EmotePtr makeBadge(const QString &id, const QJsonObject &definition)
{
    const auto name = definition.value(u"name"_s).toString().trimmed();
    if (name.isEmpty() || name.size() > 128)
    {
        return {};
    }

    const auto image1 = badgeImage(definition, u"image1x"_s, 1);
    if (image1->isEmpty())
    {
        return {};
    }

    return std::make_shared<const Emote>(Emote{
        .name = EmoteName{u"satorino:"_s + id},
        .images = ImageSet{image1, badgeImage(definition, u"image2x"_s, 2),
                           badgeImage(definition, u"image3x"_s, 4)},
        .tooltip = Tooltip{name.toHtmlEscaped()},
        .homePage = Url{},
        .id = EmoteId{id},
    });
}

bool validTwitchId(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 && id.front() != u'0' &&
           std::ranges::all_of(id, [](QChar ch) {
               return ch >= u'0' && ch <= u'9';
           });
}

}  // namespace

std::optional<satorino::BadgeAssignments> satorino::parseBadgeFeed(
    const QByteArray &body)
{
    if (body.size() > MAX_PAYLOAD_BYTES)
    {
        return std::nullopt;
    }

    const auto root = QJsonDocument::fromJson(body).object();
    const auto badgeValue = root.value(u"badges"_s);
    const auto userValue = root.value(u"users"_s);
    if (root.value(u"version"_s).toInt() != 1 || !badgeValue.isObject() ||
        !userValue.isObject())
    {
        return std::nullopt;
    }

    const auto definitions = badgeValue.toObject();
    const auto users = userValue.toObject();
    if (definitions.size() > MAX_BADGE_KINDS || users.size() > MAX_USERS)
    {
        return std::nullopt;
    }

    std::unordered_map<QString, EmotePtr> catalog;
    for (auto it = definitions.constBegin(); it != definitions.constEnd(); ++it)
    {
        const auto id = it.key();
        if (!id.isEmpty() && id.size() <= 64)
        {
            if (auto badge = makeBadge(id, it.value().toObject()))
            {
                catalog.emplace(id, std::move(badge));
            }
        }
    }

    // Keep the previous list if a nonempty catalog has no usable definitions.
    if (!definitions.isEmpty() && catalog.empty())
    {
        return std::nullopt;
    }

    BadgeAssignments assignments;
    for (auto it = users.constBegin(); it != users.constEnd(); ++it)
    {
        const auto id = it.key();
        if (!validTwitchId(id))
        {
            continue;
        }
        if (const auto badge = catalog.find(it.value().toString());
            badge != catalog.end())
        {
            assignments.emplace(id, badge->second);
        }
    }
    return assignments;
}

void SatorinoBadges::initialize()
{
    this->refreshTimer_.setSingleShot(true);
    this->refreshTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->refreshTimer_, &QTimer::timeout, this,
                     &SatorinoBadges::refresh);
    getSettings()->showBadgesSatorino.connect(
        [this](bool enabled, auto) {
            if (enabled)
            {
                this->refresh();
            }
        },
        this->connections_);
}

EmotePtr SatorinoBadges::getBadge(const QString &twitchId) const
{
    std::shared_lock lock(this->mutex_);
    const auto it = this->users_.find(twitchId);
    return it == this->users_.end() ? nullptr : it->second;
}

void SatorinoBadges::refresh()
{
    if (this->requestInFlight_ || this->refreshTimer_.isActive() ||
        !getSettings()->showBadgesSatorino || isAppAboutToQuit())
    {
        return;
    }

    this->requestInFlight_ = true;
    auto request = NetworkRequest(BADGE_FEED_URL)
                       .header("Accept", "application/json")
                       .attribute(QNetworkRequest::CacheLoadControlAttribute,
                                  QNetworkRequest::AlwaysNetwork)
                       .followRedirects(false)
                       .timeout(10000)
                       .caller(this);
    if (!this->etag_.isEmpty())
    {
        request = std::move(request).header("If-None-Match", this->etag_);
    }

    const auto handle = [this](const NetworkResult &result) {
        this->handleResponse(result);
    };
    std::move(request).onSuccess(handle).onError(handle).execute();
}

void SatorinoBadges::handleResponse(const NetworkResult &result)
{
    this->requestInFlight_ = false;
    if (result.status() == 304 && !this->etag_.isEmpty())
    {
        this->refreshTimer_.start(REFRESH_INTERVAL_MS);
        return;
    }

    if (result.status() == 200 && result.error() == QNetworkReply::NoError)
    {
        if (auto users = satorino::parseBadgeFeed(result.getData()))
        {
            {
                std::unique_lock lock(this->mutex_);
                this->users_.swap(*users);
            }
            this->etag_ = result.etag();
            this->refreshTimer_.start(REFRESH_INTERVAL_MS);
            return;
        }
        qCWarning(chatterinoApp)
            << "[satorino] Ignoring an invalid badge feed.";
    }
    else
    {
        qCWarning(chatterinoApp)
            << "[satorino] Badge feed request failed:" << result.formatError();
    }

    this->refreshTimer_.start(RETRY_INTERVAL_MS);
}

}  // namespace chatterino
