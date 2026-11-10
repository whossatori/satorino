// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "providers/cyex/CyexBadges.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "messages/Image.hpp"
#include "singletons/Settings.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>
#include <QUrl>

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace chatterino {
namespace {

using namespace literals;

constexpr qint64 REFRESH_INTERVAL_MS = 30 * 60 * 1000;
constexpr qsizetype MAX_PAYLOAD_BYTES = 8 * 1024 * 1024;
constexpr qsizetype MAX_KINDS = 64;
constexpr qsizetype MAX_USERS = 100000;
constexpr QSize BASE_SIZE(18, 18);

QJsonObject readObject(const QByteArray &body)
{
    if (body.size() > MAX_PAYLOAD_BYTES)
    {
        return {};
    }
    return QJsonDocument::fromJson(body).object();
}

bool validTwitchId(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 &&
           std::ranges::all_of(id, [](QChar ch) {
               return ch >= u'0' && ch <= u'9';
           });
}

ImagePtr badgeImage(const QJsonObject &images, const QString &key, int scale)
{
    const auto text = images.value(key).toString();
    const QUrl url(text, QUrl::StrictMode);
    if (text.size() > 2048 || !url.isValid() || url.scheme() != u"https"_s ||
        url.host() != u"api.cyex.app"_s || url.port(443) != 443 ||
        !url.userInfo().isEmpty())
    {
        return Image::getEmpty();
    }
    // Keep the supplied version or content hash as part of the cache key.
    return Image::fromUrl(Url{text}, 1.0 / scale, BASE_SIZE * scale);
}

QString badgeTooltip(const cyex::BadgeKind &kind, qint64 number)
{
    auto tooltip = kind.title.toHtmlEscaped();
    if (number > 0)
    {
        tooltip += u" #"_s + QString::number(number);
    }
    if (!kind.description.isEmpty())
    {
        tooltip += u"<br>"_s + kind.description.toHtmlEscaped();
    }
    return tooltip;
}

EmotePtr makeBadge(const QString &id, const cyex::BadgeKind &kind,
                   const QJsonObject &images)
{
    const auto image1 = badgeImage(images, u"1x"_s, 1);
    if (image1->isEmpty())
    {
        return {};
    }

    return std::make_shared<const Emote>(Emote{
        .name = EmoteName{u"cyex:"_s + id},
        // ImageSet's third slot is the 4x variant (72 px for an 18 px badge).
        .images = ImageSet{image1, badgeImage(images, u"2x"_s, 2),
                           badgeImage(images, u"4x"_s, 4)},
        .tooltip = Tooltip{badgeTooltip(kind, 0)},
        .homePage = Url{},
        .id = EmoteId{id},
    });
}

}  // namespace

std::optional<cyex::Catalog> cyex::parseCatalog(const QByteArray &body)
{
    const auto badgeValue = readObject(body).value(u"badges"_s);
    if (!badgeValue.isArray())
    {
        return std::nullopt;
    }
    const auto badges = badgeValue.toArray();
    if (badges.size() > MAX_KINDS)
    {
        return std::nullopt;
    }

    Catalog catalog;
    for (const auto &value : badges)
    {
        const auto object = value.toObject();
        const auto id = object.value(u"id"_s).toString();
        const auto title = object.value(u"title"_s).toString().trimmed();
        if (id.isEmpty() || id.size() > 64 || title.isEmpty())
        {
            continue;
        }
        BadgeKind kind{title.left(128),
                       object.value(u"description"_s).toString().left(512),
                       {}};
        if (id != u"custom"_s)
        {
            kind.badge =
                makeBadge(id, kind, object.value(u"images"_s).toObject());
            if (!kind.badge)
            {
                continue;
            }
        }
        catalog.emplace(id, std::move(kind));
    }

    // A bad catalog should be retried, including a nonempty list with no
    // usable definitions.
    if (!badges.isEmpty() && catalog.empty())
    {
        return std::nullopt;
    }
    return catalog;
}

std::optional<cyex::UserBadges> cyex::parseUsers(const QByteArray &body,
                                                 const Catalog &catalog)
{
    const auto userValue = readObject(body).value(u"users"_s);
    if (!userValue.isObject())
    {
        return std::nullopt;
    }
    const auto users = userValue.toObject();
    if (users.size() > MAX_USERS)
    {
        return std::nullopt;
    }

    UserBadges assigned;
    for (auto it = users.constBegin(); it != users.constEnd(); ++it)
    {
        if (!validTwitchId(it.key()) || !it.value().isArray())
        {
            continue;
        }
        const auto assignments = it.value().toArray();
        if (assignments.size() > MAX_KINDS)
        {
            continue;
        }

        std::vector<EmotePtr> badges;
        for (const auto &assignment : assignments)
        {
            const auto object = assignment.toObject();
            const auto id = object.value(u"id"_s).toString();
            const auto kind = catalog.find(id);
            if (kind == catalog.end() ||
                std::ranges::any_of(badges, [&](const EmotePtr &badge) {
                    return badge->id.string == id;
                }))
            {
                continue;
            }

            auto badge = kind->second.badge;
            if (id == u"custom"_s)
            {
                badge = makeBadge(id, kind->second,
                                  object.value(u"images"_s).toObject());
            }
            else if (const auto number = object.value(u"number"_s).toInteger();
                     number > 0)
            {
                auto numbered = *badge;
                numbered.tooltip = Tooltip{badgeTooltip(kind->second, number)};
                badge = std::make_shared<const Emote>(std::move(numbered));
            }
            if (badge)
            {
                badges.push_back(std::move(badge));
            }
        }

        if (!badges.empty())
        {
            assigned.emplace(it.key(), std::move(badges));
        }
    }
    return assigned;
}

qint64 cyex::retryAfterDelayMs(const QByteArray &header, const QDateTime &now)
{
    const auto value = header.trimmed();
    if (!value.isEmpty() && std::ranges::all_of(value, [](char ch) {
            return ch >= '0' && ch <= '9';
        }))
    {
        constexpr auto maxDelay = std::numeric_limits<qint64>::max();
        bool ok = false;
        const auto seconds = value.toLongLong(&ok);
        if (!ok || seconds > maxDelay / 1000)
        {
            return maxDelay;
        }
        return std::max<qint64>(1000, seconds * 1000);
    }

    auto date = QString::fromLatin1(value);
    if (date.endsWith(u" GMT"_s))
    {
        // HTTP dates use GMT. Qt's RFC 2822 parser expects a numeric offset.
        date.chop(3);
        date += u"+0000"_s;
    }
    const auto time = QDateTime::fromString(date, Qt::RFC2822Date);
    return time.isValid() ? std::max<qint64>(1000, now.msecsTo(time))
                          : REFRESH_INTERVAL_MS;
}

void CyexBadges::initialize()
{
    this->refreshTimer_.setSingleShot(true);
    this->refreshTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->refreshTimer_, &QTimer::timeout, this, [this] {
        if (this->remainingDelayMs_ > 0)
        {
            this->schedule(this->remainingDelayMs_);
            return;
        }
        this->refresh();
    });

    getSettings()->showBadgesCyex.connect(
        [this](bool enabled, auto) {
            if (enabled)
            {
                this->refresh();
            }
        },
        this->connections_);
}

std::vector<EmotePtr> CyexBadges::getBadges(const QString &twitchId) const
{
    std::shared_lock lock(this->mutex_);
    const auto it = this->users_.find(twitchId);
    return it == this->users_.end() ? std::vector<EmotePtr>{} : it->second;
}

void CyexBadges::refresh()
{
    if (this->requestInFlight_ || this->refreshTimer_.isActive() ||
        !getSettings()->showBadgesCyex || isAppAboutToQuit())
    {
        return;
    }

    this->requestInFlight_ = true;
    auto request =
        NetworkRequest(this->catalog_ ? "https://api.cyex.app/badges/v1/users"
                                      : "https://api.cyex.app/badges/v1/badges")
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

void CyexBadges::handleResponse(const NetworkResult &result)
{
    this->requestInFlight_ = false;
    if (isAppAboutToQuit())
    {
        return;
    }

    if (result.status() == 429)
    {
        this->schedule(cyex::retryAfterDelayMs(
            result.retryAfter(), QDateTime::currentDateTimeUtc()));
        return;
    }

    if (result.status() == 200 && result.error() == QNetworkReply::NoError)
    {
        if (!this->catalog_)
        {
            this->catalog_ = cyex::parseCatalog(result.getData());
            if (this->catalog_)
            {
                this->refresh();
                return;
            }
            qCWarning(chatterinoApp) << "[Cyex] Ignoring an invalid catalog.";
        }
        else if (auto users =
                     cyex::parseUsers(result.getData(), *this->catalog_))
        {
            std::unique_lock lock(this->mutex_);
            this->users_.swap(*users);
            this->etag_ = result.etag();
        }
        else
        {
            qCWarning(chatterinoApp) << "[Cyex] Ignoring an invalid user list.";
        }
    }
    else if (result.status() != 304 || this->etag_.isEmpty())
    {
        qCWarning(chatterinoApp)
            << "[Cyex] Badge request failed:" << result.formatError();
    }

    this->schedule(REFRESH_INTERVAL_MS);
}

void CyexBadges::schedule(qint64 delayMs)
{
    // QTimer takes an int in milliseconds. A longer Retry-After is split
    // into timer intervals without sending an early request.
    const auto interval = static_cast<int>(
        std::min<qint64>(delayMs, std::numeric_limits<int>::max()));
    this->remainingDelayMs_ = delayMs - interval;
    this->refreshTimer_.start(interval);
}

}  // namespace chatterino
