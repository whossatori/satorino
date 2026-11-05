// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoLive.hpp"
#include "widgets/NotebookEnums.hpp"

#include <QColor>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QVector>

#include <array>
#include <optional>

class QPainter;

namespace chatterino::satorino {

enum class IndicatorShape { Circle, Star };
inline constexpr qreal INDICATOR_STAR_FACTOR = 1.45;
IndicatorShape indicatorShape(LiveIndicator kind, bool gameStar);
// Exact stock integer geometry, including location/selection translation.
QRect indicatorRect(const QRect &tab, float scale, NotebookTabLocation location,
                    bool selected);
std::array<QPointF, 10> indicatorStar(const QPointF &centre, qreal outerRadius,
                                      qreal innerRatio = 0.5);
qreal indicatorStarRadius(const QRect &circle, const QRectF &tab,
                          const QVector<QRectF> &obstacles, qreal penWidth = 1,
                          qreal clearance = 1);
// One documented factor: the pin glyph is 15% smaller; its hit target stays usable.
inline constexpr qreal TAB_PIN_SCALE = 0.85;
QRect pinnedTabRect(const QRect &tab, float scale, bool compact);
QRect pinnedTabTitleRect(QRect title, const QRect &pin, int gap);
QRectF pinnedTabGlyphBounds(const QRect &pin, float scale);
// Uses the existing stock pen geometry; restores all painter state afterwards.
void paintHighlightedIndicator(QPainter &painter, const QRect &rect,
                               const QColor &chosen, const QColor &background,
                               IndicatorShape shape,
                               std::optional<qreal> starRadius = std::nullopt);

}  // namespace chatterino::satorino
