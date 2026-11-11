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

#include <QJsonArray>
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

constexpr auto BADGE_FEED_URL = "https://satorino.satos.cc/api/badges/v2";
constexpr auto LEGACY_FEED_URL = "https://satorino.satos.cc/api/badges";
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
    const auto version = root.value(u"version"_s).toInt();
    if ((version != 1 && version != 2) || !badgeValue.isObject() ||
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
        if (version == 1)
        {
            if (const auto badge = catalog.find(it.value().toString());
                badge != catalog.end())
            {
                assignments.emplace(id, std::vector<EmotePtr>{badge->second});
            }
            continue;
        }

        const auto choices = it.value().toArray();
        if (choices.size() > MAX_BADGE_KINDS)
        {
            continue;
        }

        std::vector<EmotePtr> badges;
        for (const auto &choice : choices)
        {
            const auto badge = catalog.find(choice.toString());
            if (badge != catalog.end() &&
                std::ranges::find(badges, badge->second) == badges.end())
            {
                badges.push_back(badge->second);
            }
        }
        if (!badges.empty())
        {
            assignments.emplace(id, std::move(badges));
        }
    }
    return assignments;
}

void SatorinoBadges::initialize()
{
    getSettings()->showBadgesSatorino.connect(
        [this](bool enabled, auto) {
            if (enabled)
            {
                this->refresh();
            }
        },
        this->connections_);
}

std::vector<EmotePtr> SatorinoBadges::getBadges(const QString &twitchId) const
{
    std::shared_lock lock(this->mutex_);
    const auto it = this->users_.find(twitchId);
    return it == this->users_.end() ? std::vector<EmotePtr>{} : it->second;
}

void SatorinoBadges::refresh()
{
    if (this->requestInFlight_ || !getSettings()->showBadgesSatorino ||
        isAppAboutToQuit())
    {
        return;
    }

    this->requestInFlight_ = true;
    this->requestFeed(false);
}

void SatorinoBadges::requestFeed(bool legacy)
{
    auto request = NetworkRequest(legacy ? LEGACY_FEED_URL : BADGE_FEED_URL)
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

    const auto handle = [this, legacy](const NetworkResult &result) {
        this->handleResponse(result, legacy);
    };
    std::move(request).onSuccess(handle).onError(handle).execute();
}

void SatorinoBadges::handleResponse(const NetworkResult &result, bool legacy)
{
    if (!legacy && result.status() == 404 && getSettings()->showBadgesSatorino)
    {
        // The previous server release serves only version 1.
        this->requestFeed(true);
        return;
    }

    this->requestInFlight_ = false;
    if (result.status() == 304 && !this->etag_.isEmpty())
    {
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
}

}  // namespace chatterino
