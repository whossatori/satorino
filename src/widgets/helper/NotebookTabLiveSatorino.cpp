// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTab.hpp"

#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "util/MultiChannel.hpp"
#include "util/SatorinoLive.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPen>  // satorino: complete paint types, independent of transitive headers
#include <QPointF>
#include <QRectF>

#include <memory>

namespace chatterino {

bool NotebookTab::paintGameIndicator(QPainter &painter, const QRect &dot) const
{
    const auto games = getSettings()->satorinoLiveGames.getValue();
    auto *container = dynamic_cast<SplitContainer *>(this->page);
    if (!this->isLive_ || games.trimmed().isEmpty() || !container)
    {
        return false;
    }
    const auto matches = [&games](const ChannelPtr &channel) {
        const auto twitch = std::dynamic_pointer_cast<TwitchChannel>(channel);
        if (!twitch || !twitch->isLive() || twitch->isRerun())
        {
            return false;
        }
        const auto status = twitch->accessStreamStatus();
        return satorino::gameMatches(games, status->game, status->gameId);
    };
    bool highlighted = false;
    for (auto *split : container->getSplits())
    {
        const auto channel = split->getChannel();
        if (auto multi = std::dynamic_pointer_cast<MultiChannel>(channel))
        {
            for (const auto &child : multi->channels())
            {
                highlighted = highlighted || matches(child.channel);
            }
        }
        else
        {
            highlighted = highlighted || matches(channel);
        }
    }
    if (satorino::liveIndicator(this->isLive_, highlighted) !=
        satorino::LiveIndicator::Game)
    {
        return false;
    }
    // Original, unbranded controller glyph in the stock indicator corner.
    painter.save();
    painter.translate(QRectF(dot).center());
    const auto scale = this->scale();
    painter.scale(scale, scale);
    auto pen = painter.pen();
    pen.setWidthF(0.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath shape;
    shape.moveTo(-2.5, -2);
    shape.cubicTo(-4, -2, -4.5, 2.5, -3.5, 2.5);
    shape.lineTo(-1.5, 1);
    shape.lineTo(1.5, 1);
    shape.lineTo(3.5, 2.5);
    shape.cubicTo(4.5, 2.5, 4, -2, 2.5, -2);
    shape.closeSubpath();
    painter.drawPath(shape);
    painter.drawLine(QPointF(-2.8, -0.5), QPointF(-1.2, -0.5));
    painter.drawLine(QPointF(-2, -1.3), QPointF(-2, 0.3));
    painter.drawPoint(QPointF(2, -1));
    painter.drawPoint(QPointF(2.8, 0));
    painter.restore();
    return true;
}

}  // namespace chatterino
