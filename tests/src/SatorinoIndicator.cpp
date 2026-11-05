// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoIndicator.hpp"

#include "util/SatorinoGames.hpp"
#include "Test.hpp"

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QtGui/qrgb.h>
#include <QTransform>

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numbers>

using namespace chatterino;
using namespace chatterino::satorino;
namespace {
constexpr std::array<float, 4> SCALES{1.0F, 1.25F, 1.5F, 2.0F};
QImage circle(float scale, qreal dpr, bool game)
{
    QImage image(static_cast<int>(std::ceil(140 * dpr)),
                 static_cast<int>(std::ceil(80 * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(Qt::red));
    painter.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));
    const auto rect = indicatorRect(QRect(0, 0, 120, 56), scale,
                                    NotebookTabLocation::Top, true);
    if (game)
        paintHighlightedIndicator(painter, rect, QColor(Qt::cyan),
                                  QColor(Qt::black), IndicatorShape::Circle);
    else
        painter.drawEllipse(rect);
    painter.end();
    return image;
}
}  // namespace

TEST(SatorinoIndicator, ExactStockRectangleAtAllRequestedScales)
{
    const QRect tab(0, 0, 120, 56);
    // Frozen stock QRect construction: float arithmetic, integer truncation.
    const std::array<QRect, 4> expected{
        QRect(113, 4, 4, 4), QRect(111, 5, 5, 5), QRect(109, 6, 6, 6),
        QRect(106, 8, 8, 8)};
    for (std::size_t i = 0; i < SCALES.size(); ++i)
    {
        SCOPED_TRACE(SCALES[i]);
        const auto stock =
            indicatorRect(tab, SCALES[i], NotebookTabLocation::Top, true);
        EXPECT_EQ(stock, expected[i]);
        EXPECT_EQ(indicatorRect(tab.translated(10, 20), SCALES[i],
                                NotebookTabLocation::Top, true),
                  stock.translated(10, 20));
    }
}
TEST(SatorinoIndicator, StockSelectionAndLocationOffsetsArePreserved)
{
    const std::array<NotebookTabLocation, 4> locations{Top, Left, Right,
                                                       Bottom};
    const std::array<QPoint, 4> offsets{QPoint(0, 1), QPoint(1, 0),
                                        QPoint(-1, 0), QPoint(0, -1)};
    for (float scale : SCALES)
        for (std::size_t i = 0; i < locations.size(); ++i)
        {
            const auto selected =
                indicatorRect(QRect(0, 0, 120, 56), scale, locations[i], true);
            const auto unselected =
                indicatorRect(QRect(0, 0, 120, 56), scale, locations[i], false);
            EXPECT_EQ(unselected, selected.translated(offsets[i]));
        }
}
TEST(SatorinoIndicator, StarHasTenAlternatingVerticesAndPointsUp)
{
    const QPointF centre(12, 15);
    const auto points = indicatorStar(centre, 10, 0.48);
    ASSERT_EQ(points.size(), 10U);
    EXPECT_NEAR(points[0].x(), centre.x(), 1e-9);
    EXPECT_NEAR(points[0].y(), centre.y() - 10, 1e-9);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto delta = points[i] - centre;
        EXPECT_NEAR(std::hypot(delta.x(), delta.y()), i % 2 == 0 ? 10 : 4.8,
                    1e-9);
    }
}
TEST(SatorinoIndicator, StarHasMirrorAndFivefoldRotationalSymmetry)
{
    const QPointF centre(9, 11);
    const auto points = indicatorStar(centre, 7);
    const qreal angle = 2 * std::numbers::pi_v<qreal> / 5;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto mirror = points[(10 - i) % 10];
        EXPECT_NEAR(points[i].x() + mirror.x(), 2 * centre.x(), 1e-9);
        EXPECT_NEAR(points[i].y(), mirror.y(), 1e-9);
        const auto delta = points[i] - centre;
        const auto rotated =
            centre +
            QPointF(delta.x() * std::cos(angle) - delta.y() * std::sin(angle),
                    delta.x() * std::sin(angle) + delta.y() * std::cos(angle));
        EXPECT_NEAR(rotated.x(), points[(i + 2) % 10].x(), 1e-9);
        EXPECT_NEAR(rotated.y(), points[(i + 2) % 10].y(), 1e-9);
    }
}
TEST(SatorinoIndicator, StarFitsTheStockPaintedBoxAtEveryScale)
{
    for (float scale : SCALES)
        for (bool selected : {false, true})
            for (auto location : {Top, Left, Right, Bottom})
            {
                // Stock's one-unit pen extends half a unit beyond each rectangle edge.
                const auto box =
                    QRectF(indicatorRect(QRect(0, 0, 120, 56), scale, location,
                                         selected))
                        .adjusted(-0.5, -0.5, 0.5, 0.5);
                const auto points =
                    indicatorStar(box.center(), box.width() / 2);
                for (const auto &point : points)
                {
                    EXPECT_GE(point.x(), box.left() - 1e-9);
                    EXPECT_LE(point.x(), box.right() + 1e-9);
                    EXPECT_GE(point.y(), box.top() - 1e-9);
                    EXPECT_LE(point.y(), box.bottom() + 1e-9);
                }
            }
}
TEST(SatorinoIndicator, ShapeChooserUsesOptionOnlyForGameKind)
{
    for (bool enabled : {false, true})
        for (bool live : {false, true})
            for (bool match : {false, true})
            {
                const auto kind = liveIndicator(live, match);
                EXPECT_EQ(indicatorShape(kind, enabled),
                          live && match && enabled ? IndicatorShape::Star
                                                   : IndicatorShape::Circle);
                EXPECT_EQ(
                    indicatorShape(liveIndicator(live, match, true), enabled),
                    IndicatorShape::Circle);
            }
    EXPECT_EQ(indicatorShape(LiveIndicator::Train, true),
              IndicatorShape::Circle);
}
TEST(SatorinoIndicator, ColoursStillResolveByPriorityIndependentlyOfShape)
{
    IndicatorColours colours{QColor(Qt::red), QColor(Qt::cyan), {}};
    for (bool enabled : {false, true})
    {
        EXPECT_EQ(indicatorShape(LiveIndicator::Game, enabled),
                  enabled ? IndicatorShape::Star : IndicatorShape::Circle);
        EXPECT_EQ(indicatorColour(LiveIndicator::Game, colours), colours.game);
        EXPECT_EQ(indicatorColour(LiveIndicator::Dot, colours), colours.stock);
        EXPECT_FALSE(indicatorColour(LiveIndicator::None, colours));
        EXPECT_EQ(indicatorColour(LiveIndicator::Train, colours),
                  colours.stock);
    }
}
TEST(SatorinoIndicator, ShadeKeepsVisibleColoursAndAdaptsLowContrastFills)
{
    EXPECT_EQ(indicatorFillColour(QColor(Qt::cyan), QColor(Qt::black)),
              QColor(Qt::cyan));
    EXPECT_EQ(indicatorFillColour(QColor(Qt::blue), QColor(Qt::white)),
              QColor(Qt::blue));
    const auto dark = indicatorFillColour(QColor("#101820"), QColor("#101820"));
    EXPECT_NE(dark, QColor("#101820"));
    EXPECT_GT(dark.lightness(), QColor("#101820").lightness());
    EXPECT_LE(dark.red(), dark.green());
    EXPECT_LE(dark.green(), dark.blue());
    const auto light =
        indicatorFillColour(QColor(Qt::white), QColor(Qt::white));
    EXPECT_LT(light.lightness(), 255);
    EXPECT_EQ(dark.alpha(), 255);
    EXPECT_EQ(light.alpha(), 255);
}
TEST(SatorinoIndicator, ContrastBackgroundIncludesCustomTabOverlay)
{
    const auto base = QColor(Qt::blue);
    EXPECT_EQ(indicatorBackground(base, {}), base);
    const auto composite = indicatorBackground(base, QColor(255, 0, 0, 128));
    EXPECT_NEAR(composite.red(), 128, 1);
    EXPECT_EQ(composite.green(), 0);
    EXPECT_NEAR(composite.blue(), 127, 1);
}
TEST(SatorinoIndicator, GameCircleHasExactStockCoverageAndNoInsetBorder)
{
    for (float scale : SCALES)
        for (qreal dpr : {1.0, 1.25, 1.5, 2.0})
        {
            SCOPED_TRACE(scale);
            SCOPED_TRACE(dpr);
            const auto stock = circle(scale, dpr, false);
            const auto game = circle(scale, dpr, true);
            ASSERT_EQ(stock.size(), game.size());
            for (int y = 0; y < stock.height(); ++y)
                for (int x = 0; x < stock.width(); ++x)
                {
                    ASSERT_EQ(qAlpha(stock.pixel(x, y)),
                              qAlpha(game.pixel(x, y)));
                    if (qAlpha(game.pixel(x, y)) >= 192)
                    {
                        const auto pixel = game.pixelColor(x, y);
                        EXPECT_NEAR(pixel.red(), 0, 2);
                        EXPECT_NEAR(pixel.green(), 255, 2);
                        EXPECT_NEAR(pixel.blue(), 255, 2);
                    }
                }
        }
}
TEST(SatorinoIndicator, PaintingRestoresPenBrushTransformAndAntialiasing)
{
    QImage image(120, 56, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setPen(QPen(QBrush(QColor(Qt::red)), 1.0));
    painter.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));
    const auto pen = painter.pen();
    const auto brush = painter.brush();
    const auto transform = painter.transform();
    const auto hints = painter.renderHints();
    for (auto shape : {IndicatorShape::Circle, IndicatorShape::Star})
    {
        paintHighlightedIndicator(painter, QRect(110, 4, 4, 4),
                                  QColor(Qt::cyan), QColor(Qt::black), shape);
        EXPECT_EQ(painter.pen(), pen);
        EXPECT_EQ(painter.brush(), brush);
        EXPECT_EQ(painter.transform(), transform);
        EXPECT_EQ(painter.renderHints(), hints);
    }
}
