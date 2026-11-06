// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTab.hpp"

#include "controllers/trains/SatorinoTrainProducer.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/MultiChannel.hpp"
#include "util/SatorinoGames.hpp"
#include "util/SatorinoTrain.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <QColor>
#include <QDateTime>
#include <QVector>

#include <memory>
#include <optional>

namespace chatterino {
std::optional<QColor> NotebookTab::gameIndicatorColour() const
{
    const auto games = getSettings()->satorinoLiveGamesV2.getValue();
    auto *container = dynamic_cast<SplitContainer *>(this->page);
    if (!this->isLive_ || games.isEmpty() || !container)
        return std::nullopt;
    QVector<satorino::LiveGame> live;
    const auto collect = [&live](const ChannelPtr &channel) {
        const auto twitch = std::dynamic_pointer_cast<TwitchChannel>(channel);
        if (!twitch || !twitch->isLive() || twitch->isRerun())
            return;
        const auto status = twitch->accessStreamStatus();
        live.push_back({status->gameId, status->game});
    };
    for (auto *split : container->getSplits())
    {
        const auto channel = split->getChannel();
        if (auto multi = std::dynamic_pointer_cast<MultiChannel>(channel))
        {
            for (const auto &child : multi->channels())
                collect(child.channel);
        }
        else
            collect(channel);
    }
    const auto match = satorino::firstMatchingGame(games, live);
    // Unmatched dots follow the exact stock paint path.
    if (!match)
        return std::nullopt;
    return satorino::indicatorColour(
        satorino::liveIndicator(true, true),
        satorino::IndicatorColours{
            this->theme->tabs.liveIndicator, games[*match].colour, {}});
}
void NotebookTab::bindTrainIndicator()
{
    getSettings()->satorinoLiveTrainEnabled.connect(
        [this](bool) {
            this->update();
        },
        this->managedConnections_);
    getSettings()->satorinoLiveTrainColours.connect(
        [this](const satorino::TrainColours &) {
            if (getSettings()->satorinoLiveTrainEnabled.getValue())
                this->update();
        },
        this->managedConnections_);
    this->managedConnections_.managedConnect(
        satorino::trainProducer().changed, [this](const QString &) {
            assertInGuiThread();
            if (getSettings()->satorinoLiveTrainEnabled.getValue())
                this->update();
        });
}
std::optional<satorino::HighlightedIndicator>
    NotebookTab::highlightedLiveIndicator() const
{
    const auto game = this->gameIndicatorColour();
    // Disabled/unavailable means only the pre-existing game/dot path runs.
    if (!this->isLive_ || !getSettings()->satorinoLiveTrainEnabled.getValue() ||
        !satorino::trainProducer().available())
    {
        if (game)
            return satorino::HighlightedIndicator{
                satorino::LiveIndicator::Game,
                satorino::indicatorShape(
                    satorino::LiveIndicator::Game,
                    getSettings()->satorinoLiveGameStar.getValue()),
                *game};
        return std::nullopt;
    }
    const auto now = QDateTime::currentMSecsSinceEpoch();
    QVector<satorino::TrainState> trains;
    auto *container = dynamic_cast<SplitContainer *>(this->page);
    // Current no-data producer short-circuits: no channel scan, retained state or transport.
    if (container)
    {
        const auto collect = [&trains, now](const ChannelPtr &channel) {
            const auto twitch =
                std::dynamic_pointer_cast<TwitchChannel>(channel);
            if (twitch && channel->getType() == Channel::Type::Twitch &&
                twitch->isLive() && !twitch->isRerun() &&
                !twitch->roomId().isEmpty())
                if (const auto state = satorino::trainProducer().snapshot(
                        twitch->roomId(), now))
                    trains.push_back(*state);
        };
        for (auto *split : container->getSplits())
        {
            const auto channel = split->getChannel();
            if (const auto multi =
                    std::dynamic_pointer_cast<MultiChannel>(channel))
                for (const auto &child : multi->channels())
                    collect(child.channel);
            else
                collect(channel);
        }
    }
    return satorino::highlightedLiveIndicator(
        this->isLive_, game, getSettings()->satorinoLiveGameStar.getValue(),
        true, trains, getSettings()->satorinoLiveTrainColours.getValue(), now);
}
}  // namespace chatterino
