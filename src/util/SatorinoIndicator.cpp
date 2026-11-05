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
namespace {
qreal luminance(const QColor &colour)
{
    const auto linear = [](qreal c) {
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(colour.redF()) + 0.7152 * linear(colour.greenF()) +
           0.0722 * linear(colour.blueF());
}
qreal contrast(qreal a, qreal b)
{
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}
}  // namespace

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
                            : 0.48;
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

QColor indicatorBackground(const QColor &base, const QColor &overlay)
{
    if (!overlay.isValid())
        return base;
    const qreal alpha = overlay.alphaF();
    return QColor::fromRgbF(
        base.redF() * (1 - alpha) + overlay.redF() * alpha,
        base.greenF() * (1 - alpha) + overlay.greenF() * alpha,
        base.blueF() * (1 - alpha) + overlay.blueF() * alpha);
}

QColor indicatorFillColour(const QColor &chosen, const QColor &background)
{
    const qreal bg = luminance(background);
    if (contrast(luminance(chosen), bg) >= 3)
        return chosen;
    // Blend toward the better-contrasting endpoint, preserving the selected hue.
    const qreal target = contrast(1, bg) > contrast(0, bg) ? 1 : 0;
    for (int step = 1; step <= 20; ++step)
    {
        const qreal amount = step / 20.0;
        const auto shade =
            QColor::fromRgbF(chosen.redF() * (1 - amount) + target * amount,
                             chosen.greenF() * (1 - amount) + target * amount,
                             chosen.blueF() * (1 - amount) + target * amount);
        if (contrast(luminance(shade), bg) >= 3)
            return shade;
    }
    return chosen;
}

void paintHighlightedIndicator(QPainter &painter, const QRect &rect,
                               const QColor &chosen, const QColor &background,
                               IndicatorShape shape)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const auto fill = indicatorFillColour(chosen, background);
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
        // Use the stock ellipse's outer stroke footprint, with no pen to blunt the points.
        // This adds no painted extent in compact title/close/pin layouts.
        painter.setPen(Qt::NoPen);
        const QRectF bounds(rect);
        const auto points = indicatorStar(
            bounds.center(),
            (std::min(bounds.width(), bounds.height()) + pen.widthF()) / 2);
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
