// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/notifications/NotificationController.hpp"

#include "controllers/notifications/SatorinoLiveNotifications.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/Settings.hpp"  // satorino: enrich migrated game names from existing polls

#include <QStringList>  // satorino: direct adapter dependency

#include <utility>

namespace chatterino {

void NotificationController::syncSatorinoWatchList()
{
    if (!this->satorinoLive_)
    {
        return;
    }
    QStringList names;
    for (const auto &name : this->channelMap[Platform::Twitch].raw())
    {
        names.push_back(name);
    }
    this->satorinoLive_->setWatched(names);
}

void NotificationController::observeSatorinoLive(
    const QString &login, const std::optional<HelixStream> &stream,
    bool initial)
{
    // satorino: use existing metadata only; never add a request for migration.
    if (stream)
    {
        auto games = getSettings()->satorinoLiveGamesV2.getValue();
        if (satorino::learnGame(games, stream->gameId, stream->gameName))
            getSettings()->satorinoLiveGamesV2.setValue(games);
    }
    if (!this->satorinoLive_)
    {
        this->satorinoLive_ = std::make_shared<satorino::LiveNotifications>();
    }
    this->syncSatorinoWatchList();
    this->satorinoLive_->observe(login, stream, initial);
}

void NotificationController::testSatorinoWebhook(
    std::function<void(QString)> result)
{
    if (!this->satorinoLive_)
    {
        this->satorinoLive_ = std::make_shared<satorino::LiveNotifications>();
    }
    this->satorinoLive_->test(std::move(result));
}

}  // namespace chatterino
