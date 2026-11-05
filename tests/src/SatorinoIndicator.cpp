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
#include <QPainterPath>
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
    const auto points = indicatorStar(centre, 10);
    ASSERT_EQ(points.size(), 10U);
    EXPECT_NEAR(points[0].x(), centre.x(), 1e-9);
    EXPECT_NEAR(points[0].y(), centre.y() - 10, 1e-9);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto delta = points[i] - centre;
        EXPECT_NEAR(std::hypot(delta.x(), delta.y()), i % 2 == 0 ? 10 : 5.0,
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
TEST(SatorinoIndicator, StarUsesOnePointFourFiveRadiusWhenThereIsRoom)
{
    const QRect circle(48, 48, 4, 4);
    const auto radius = indicatorStarRadius(circle, QRectF(0, 0, 100, 100), {});
    EXPECT_NEAR(radius, 2.5 * 1.45, 1e-9);
    EXPECT_NEAR(radius, 2.5 * INDICATOR_STAR_FACTOR, 1e-9);
}
TEST(SatorinoIndicator, StarClampsInsideTabAndAvoidsTitleCloseAndPinFixtures)
{
    for (float scale : SCALES)
        for (bool compact : {false, true})
            for (bool pinned : {false, true})
                for (bool close : {false, true})
                    for (bool selected : {false, true})
                        for (auto location : {Top, Left, Right, Bottom})
                        {
                            const QRect tab(
                                0, 0,
                                static_cast<int>((compact ? 84 : 120) * scale),
                                static_cast<int>(28 * scale));
                            const auto circle =
                                indicatorRect(tab, scale, location, selected);
                            const auto centre = QRectF(circle).center();
                            // Conservative title ink fixtures include long titles and close-button-off layouts.
                            QVector<QRectF> obstacles{
                                QRectF(4 * scale, 10 * scale,
                                       tab.width() -
                                           (close || pinned ? 30 : 8) * scale,
                                       13 * scale)};
                            const int size = static_cast<int>(16 * scale);
                            const int adjustment =
                                location == Top ? size / 3 : size / 2;
                            QRect controls(
                                tab.right() - static_cast<int>(
                                                  (compact ? 16 : 20) * scale),
                                tab.center().y() - adjustment, size, size);
                            if (selected)
                            {
                                if (location == Top)
                                    controls.translate(0, -1);
                                else if (location == Bottom)
                                    controls.translate(0, 1);
                                else if (location == Left)
                                    controls.translate(-1, 0);
                                else
                                    controls.translate(1, 0);
                            }
                            if (pinned)
                            {
                                controls = pinnedTabRect(tab, scale, compact);
                                obstacles[0] = QRectF(pinnedTabTitleRect(
                                    obstacles[0].toRect(), controls,
                                    int(2 * scale)));
                                obstacles.push_back(
                                    pinnedTabGlyphBounds(controls, scale));
                            }
                            else if (close)
                            {
                                const int a = static_cast<int>(4 * scale);
                                obstacles.push_back(
                                    QRectF(controls.adjusted(a, a, -a, -a))
                                        .adjusted(-0.5, -0.5, 0.5, 0.5));
                            }
                            const qreal radius = indicatorStarRadius(
                                circle, QRectF(tab), obstacles, 1, 0.5);
                            EXPECT_GE(radius, 0);
                            EXPECT_LE(radius, (circle.width() + 1) / 2.0 *
                                                  INDICATOR_STAR_FACTOR);
                            // The renderer suppresses a fully obstructed star.
                            if (radius == 0)
                                continue;
                            const auto points = indicatorStar(centre, radius);
                            QPainterPath star;
                            star.moveTo(points.front());
                            for (std::size_t i = 1; i < points.size(); ++i)
                                star.lineTo(points[i]);
                            star.closeSubpath();
                            for (const auto &p : points)
                            {
                                EXPECT_GE(p.x(), -1e-9);
                                EXPECT_LE(p.x(), tab.width() + 1e-9);
                                EXPECT_GE(p.y(), -1e-9);
                                EXPECT_LE(p.y(), tab.height() + 1e-9);
                            }
                            for (const auto &obstacle : obstacles)
                                EXPECT_FALSE(star.intersects(obstacle));
                        }
}
TEST(SatorinoIndicator, StarShrinksOrHidesInsteadOfOverlappingAnObstruction)
{
    const QRect circle(48, 48, 4, 4);
    const QRectF tab(0, 0, 100, 100);
    EXPECT_NEAR(
        indicatorStarRadius(circle, tab, {QRectF(52, 40, 10, 20)}, 1, 0.5), 1.5,
        1e-9);
    EXPECT_EQ(indicatorStarRadius(circle, tab, {QRectF(49, 49, 2, 2)}), 0);
    EXPECT_LE(indicatorStarRadius(QRect(96, 48, 4, 4), tab, {}, 1, 0.5), 1.5);
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

TEST(SatorinoIndicator, ExactChosenPixelsForEveryBackgroundStateAndShape)
{
    for (auto shape : {IndicatorShape::Circle, IndicatorShape::Star})
        for (bool selected : {false, true})
            for (bool hover : {false, true})
                for (const auto &background :
                     {QColor("#101820"), QColor("#fafafa"), QColor("#667788")})
                {
                    QImage image(120, 56, QImage::Format_ARGB32);
                    image.fill(hover ? background.lighter() : background);
                    QPainter painter(&image);
                    painter.setPen(QColor(Qt::red));
                    painter.setBrush(QBrush(Qt::red, Qt::SolidPattern));
                    const auto rect =
                        indicatorRect(QRect(0, 0, 120, 56), 2, Top, selected);
                    const QColor chosen("#101820");
                    paintHighlightedIndicator(painter, rect, chosen, background,
                                              shape);
                    painter.end();
                    EXPECT_EQ(image.pixelColor(QRectF(rect).center().toPoint()),
                              chosen);
                }
}
TEST(SatorinoIndicator, ChosenAlphaIsPreserved)
{
    for (auto shape : {IndicatorShape::Circle, IndicatorShape::Star})
    {
        QImage image(80, 80, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setBrush(QBrush(Qt::red, Qt::SolidPattern));
        const QColor chosen(255, 0, 255, 128);
        paintHighlightedIndicator(painter, QRect(30, 30, 16, 16), chosen,
                                  Qt::white, shape);
        painter.end();
        EXPECT_EQ(image.pixelColor(38, 38), chosen);
    }
}
TEST(SatorinoIndicator, LeftPinReplacesRightReservationAndFitsTitle)
{
    EXPECT_DOUBLE_EQ(TAB_PIN_SCALE, 0.85);
    for (float scale : SCALES)
        for (bool compact : {false, true})
            for (auto location : {Top, Left, Right, Bottom})
                for (bool selected : {false, true})
                {
                    // Pin geometry is intentionally independent of location and selection.
                    (void)location;
                    (void)selected;
                    const QRect tab(0, 0, int(84 * scale), int(28 * scale));
                    const auto pin = pinnedTabRect(tab, scale, compact);
                    EXPECT_LT(pin.right(), tab.center().x());
                    EXPECT_NEAR(QRectF(pin).center().y(),
                                QRectF(tab).center().y(), 0.5);
                    EXPECT_TRUE(
                        QRectF(pin).contains(pinnedTabGlyphBounds(pin, scale)));
                    const auto title = pinnedTabTitleRect(
                        tab.adjusted(2, 0, -2, 0), pin, int(2 * scale));
                    EXPECT_GT(title.left(), pin.right());
                    EXPECT_EQ(title.right(), tab.right() - 2);
                    EXPECT_TRUE(tab.contains(pin));
                    EXPECT_FALSE(pin.contains(
                        QPoint(tab.right() - 5, tab.center().y())));
                }
}
