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
IndicatorShape indicatorShape(LiveIndicator kind, bool gameStar)
{
    // Train stays reserved; a later producer can select its own shape here.
    return kind == LiveIndicator::Game && gameStar ? IndicatorShape::Star
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
    qreal radius = (std::min(box.width(), box.height()) + penWidth) / 2 *
                   INDICATOR_STAR_FACTOR;
    radius = std::min(
        {radius, centre.x() - tab.left() - gap, tab.right() - centre.x() - gap,
         centre.y() - tab.top() - gap, tab.bottom() - centre.y() - gap});
    for (const auto &rect : obstacles)
    {
        if (rect.isEmpty())
            continue;
        const qreal dx = std::max(
            {rect.left() - centre.x(), centre.x() - rect.right(), qreal(0)});
        const qreal dy = std::max(
            {rect.top() - centre.y(), centre.y() - rect.bottom(), qreal(0)});
        // The entire star is inside this disc, so distance-to-rect safely bounds every edge.
        radius = std::min(radius, std::hypot(dx, dy) - gap);
    }
    return std::max(qreal(0), radius);
}

QRect pinnedTabRect(const QRect &tab, float scale, bool compact)
{
    const int size = static_cast<int>(16 * scale);
    // Mirror the old right margin; keep true vertical centring at every location.
    const int margin = static_cast<int>((compact ? 16 : 20) * scale) - size + 1;
    return QRect(tab.left() + margin, tab.top() + (tab.height() - size) / 2,
                 size, size);
}
QRect pinnedTabTitleRect(QRect title, const QRect &pin, int gap)
{
    title.setLeft(std::max(title.left(), pin.right() + gap));
    return title;
}
QRectF pinnedTabGlyphBounds(const QRect &pin, float scale)
{
    const auto centre = QRectF(pin).center();
    const qreal factor = scale * TAB_PIN_SCALE;
    // Original path plus the round 1.2-unit stroke, transformed exactly as paintPin.
    return QRectF(centre.x() - 4.6 * factor, centre.y() - 5.6 * factor,
                  9.2 * factor, 12.2 * factor);
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
