// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoLive.hpp"
#include "widgets/NotebookEnums.hpp"

#include <QColor>
#include <QPointF>
#include <QRect>

#include <array>

class QPainter;

namespace chatterino::satorino {

enum class IndicatorShape { Circle, Star };
IndicatorShape indicatorShape(LiveIndicator kind, bool gameStar);
// Exact stock integer geometry, including location/selection translation.
QRect indicatorRect(const QRect &tab, float scale, NotebookTabLocation location,
                    bool selected);
std::array<QPointF, 10> indicatorStar(const QPointF &centre, qreal outerRadius,
                                      qreal innerRatio = 0.48);
// Shade only when necessary: contrast without an inset outline or a size change.
QColor indicatorFillColour(const QColor &chosen, const QColor &background);
QColor indicatorBackground(const QColor &base, const QColor &overlay);
// Uses the existing stock pen geometry; restores all painter state afterwards.
void paintHighlightedIndicator(QPainter &painter, const QRect &rect,
                               const QColor &chosen, const QColor &background,
                               IndicatorShape shape);

}  // namespace chatterino::satorino
