// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTab.hpp"

#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/MultiChannel.hpp"
#include "util/SatorinoGames.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <QColor>
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
        satorino::IndicatorColours{this->theme->tabs.liveIndicator,
                                   games[*match].colour});
}
}  // namespace chatterino
