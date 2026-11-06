// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoIndicator.hpp"

#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace chatterino::satorino {
IndicatorShape indicatorShape(LiveIndicator kind, bool gameStar,
                              bool goldenKappa)
{
    return ((kind == LiveIndicator::Game && gameStar) ||
            (kind == LiveIndicator::Train && goldenKappa))
               ? IndicatorShape::Star
               : IndicatorShape::Circle;
}

QRect indicatorRect(const QRect &tab, float scale, NotebookTabLocation location,
                    bool selected)
{
    if (!tab.isValid() || !std::isfinite(scale) || scale <= 0)
        return {};
    // Keep float arithmetic and truncation from NotebookTab's stock QRect call.
    QRect rect(tab.left() + static_cast<int>(tab.width() - (7 * scale)),
               tab.top() + static_cast<int>(4 * scale),
               static_cast<int>(4 * scale), static_cast<int>(4 * scale));
    const int amount = selected ? 0 : -1;
    switch (location)
    {
        case NotebookTabLocation::Top:
            rect.translate(0, -amount);
            break;
        case NotebookTabLocation::Left:
            rect.translate(-amount, 0);
            break;
        case NotebookTabLocation::Right:
            rect.translate(amount, 0);
            break;
        case NotebookTabLocation::Bottom:
            rect.translate(0, amount);
            break;
    }
    return rect;
}

QRect trainIndicatorRect(const QRect &tab, float scale,
                         NotebookTabLocation location, bool selected)
{
    auto result = indicatorRect(tab, scale, location, selected);
    if (!result.isEmpty())
        result.moveLeft(tab.left() + tab.right() - result.right());
    return result;
}
std::optional<TrainIndicatorGeometry> trainIndicatorGeometry(
    const QRect &tab, float scale, NotebookTabLocation location, bool selected,
    IndicatorShape shape, const QVector<QRectF> &obstacles, qreal penWidth,
    qreal clearance)
{
    auto rect = trainIndicatorRect(tab, scale, location, selected);
    if (rect.isEmpty())
        return std::nullopt;
    const auto original = QRectF(rect).center();
    const auto dotRadius =
        (std::min(rect.width(), rect.height()) + penWidth) / 2.0;
    const auto floor = shape == IndicatorShape::Star
                           ? dotRadius * INDICATOR_STAR_FLOOR
                           : dotRadius;
    qreal above = original.y();
    // Keep the mirror's x. Move above overlapping pin/title ink instead of violating the floor.
    for (const auto &obstacle : obstacles)
        if (!obstacle.isEmpty() &&
            obstacle.left() < original.x() + floor + clearance &&
            obstacle.right() > original.x() - floor - clearance &&
            obstacle.bottom() > original.y() - floor - clearance)
            above = std::min(above, obstacle.top() - floor - clearance);
    rect.translate(0, static_cast<int>(std::floor(above - original.y())));
    const auto centre = QRectF(rect).center();
    const auto radius = shape == IndicatorShape::Star
                            ? indicatorStarRadius(rect, QRectF(tab), obstacles,
                                                  penWidth, clearance)
                            : dotRadius;
    const QRectF bounds(centre.x() - radius, centre.y() - radius, 2 * radius,
                        2 * radius);
    if (radius <= 0 || !QRectF(tab).contains(bounds))
        return std::nullopt;
    for (const auto &obstacle : obstacles)
        if (!obstacle.isEmpty() &&
            bounds.intersects(obstacle.adjusted(-clearance, -clearance,
                                                clearance, clearance)))
        {
            // No space even at the tab-edge clamp: never paint over ink.
            return std::nullopt;
        }
    return TrainIndicatorGeometry{rect, radius, bounds};
}
std::array<QPointF, 10> indicatorStar(const QPointF &centre, qreal outerRadius,
                                      qreal innerRatio)
{
    std::array<QPointF, 10> points{};
    const qreal radius =
        std::isfinite(outerRadius) ? std::max(qreal(0), outerRadius) : 0;
    const qreal ratio = std::isfinite(innerRatio)
                            ? std::clamp(innerRatio, qreal(0), qreal(1))
                            : 0.5;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const qreal angle =
            -std::numbers::pi_v<qreal> / 2 +
            static_cast<qreal>(i) * std::numbers::pi_v<qreal> / 5;
        const qreal r = (i % 2 == 0) ? radius : radius * ratio;
        points[i] = centre + QPointF(r * std::cos(angle), r * std::sin(angle));
    }
    return points;
}

qreal indicatorStarRadius(const QRect &circle, const QRectF &tab,
                          const QVector<QRectF> &obstacles, qreal penWidth,
                          qreal clearance)
{
    const QRectF box(circle);
    const auto centre = box.center();
    const qreal gap = std::max(qreal(0), clearance);
    const qreal paintedRadius =
        (std::min(box.width(), box.height()) + penWidth) / 2;
    const qreal floor = paintedRadius * INDICATOR_STAR_FLOOR;
    qreal radius = paintedRadius * INDICATOR_STAR_FACTOR;
    radius = std::min(
        {radius, centre.x() - tab.left() - gap, tab.right() - centre.x() - gap,
         centre.y() - tab.top() - gap, tab.bottom() - centre.y() - gap});
    const qreal edgeLimit = std::max(qreal(0), radius);
    for (const auto &rect : obstacles)
    {
        if (rect.isEmpty())
            continue;
        const qreal dx = std::max(
            {rect.left() - centre.x(), centre.x() - rect.right(), qreal(0)});
        const qreal dy = std::max(
            {rect.top() - centre.y(), centre.y() - rect.bottom(), qreal(0)});
        // Obstacle pressure stops at the Batch 7 floor; the tab edge limit stays hard.
        radius = std::min(radius, std::max(floor, std::hypot(dx, dy) - gap));
    }
    return std::max(qreal(0), std::min(radius, edgeLimit));
}

QRect pinnedTabRect(const QRect &tab, float scale, bool)
{
    const int width =
        static_cast<int>(std::ceil((9.2 * TAB_PIN_SCALE + 2) * scale));
    const int height =
        static_cast<int>(std::ceil((12.2 * TAB_PIN_SCALE + 2) * scale));
    return QRect(tab.left() + static_cast<int>(4 * scale),
                 tab.top() + (tab.height() - height) / 2, width, height);
}

QRect pinnedTabTitleRect(QRect title, const QRect &pin, int gap)
{
    title.setLeft(std::max(title.left(), pin.right() + gap));
    return title;
}
QPointF pinnedTabGlyphOrigin(const QRect &pin, float scale)
{
    // The original stroked path spans y=-5.6..6.6; centre its ink, not its origin.
    return QRectF(pin).center() - QPointF(0, 0.5 * scale * TAB_PIN_SCALE);
}
QRectF pinnedTabGlyphBounds(const QRect &pin, float scale)
{
    const auto centre = pinnedTabGlyphOrigin(pin, scale);
    const qreal factor = scale * TAB_PIN_SCALE;
    // Original path plus the round 1.2-unit stroke, transformed exactly as paintPin.
    return QRectF(centre.x() - 4.6 * factor, centre.y() - 5.6 * factor,
                  9.2 * factor, 12.2 * factor);
}

int pinnedTabWidth(int titleWidth, int height, float scale, bool compact)
{
    const auto pin = pinnedTabRect(QRect(0, 0, 1, height), scale, compact);
    const qreal rightMargin = 4 * scale / (compact ? 1.5 : 1.0);
    // One tight left reservation; full title width, no inherited close-button padding/cap.
    return std::max(height,
                    static_cast<int>(std::ceil(pin.right() + 2 * scale +
                                               titleWidth + rightMargin)));
}

QRect starSafeCloseGlyph(const QRect &hit, const QRect &circle, float scale,
                         qreal clearance)
{
    const int inset = static_cast<int>(4 * scale);
    auto glyph = hit.adjusted(inset, inset, -inset, -inset);
    const auto centre = QRectF(circle).center();
    const qreal floor = (std::min(circle.width(), circle.height()) + 1) / 2.0 *
                        INDICATOR_STAR_FLOOR;
    if (glyph.right() + 0.5 >= centre.x() - floor &&
        glyph.left() - 0.5 <= centre.x() + floor)
    {
        const int needed =
            static_cast<int>(std::ceil(centre.y() + floor + clearance + 0.5)) -
            glyph.top();
        // Keep the existing hit/hover box; use only its unused internal padding.
        const int room = std::max(0, hit.bottom() - glyph.bottom() - 1);
        glyph.translate(0, std::clamp(needed, 0, room));
    }
    return glyph;
}

void paintHighlightedIndicator(QPainter &painter, const QRect &rect,
                               const QColor &chosen, const QColor &,
                               IndicatorShape shape,
                               std::optional<qreal> starRadius)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const auto &fill =
        chosen;  // Exact stored RGB and alpha, independent of tab state.
    auto pen = painter.pen();
    // Same stock width/style/cosmetic state; no contrasting border.
    pen.setColor(fill);
    auto brush = painter.brush();
    brush.setColor(fill);
    painter.setBrush(brush);
    if (shape == IndicatorShape::Circle)
    {
        painter.setPen(pen);
        // Identical overload/rect/stroke as the stock path.
        painter.drawEllipse(rect);
    }
    else
    {
        painter.setPen(Qt::NoPen);
        const QRectF bounds(rect);
        const qreal radius = starRadius.value_or(
            (std::min(bounds.width(), bounds.height()) + pen.widthF()) / 2 *
            INDICATOR_STAR_FACTOR);
        if (radius <= 0)
        {
            painter.restore();
            return;
        }
        const auto points = indicatorStar(bounds.center(), radius);
        QPainterPath path;
        path.moveTo(points.front());
        for (std::size_t i = 1; i < points.size(); ++i)
            path.lineTo(points[i]);
        path.closeSubpath();
        painter.drawPath(path);
    }
    painter.restore();
}

}  // namespace chatterino::satorino
