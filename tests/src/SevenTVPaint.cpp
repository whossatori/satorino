// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "mocks/BaseApplication.hpp"
#include "providers/seventv/paints/Paint.hpp"
#include "Test.hpp"

#include <QPixmap>

#include <tuple>

namespace chatterino {

namespace {

class CountingPaint : public Paint
{
public:
    CountingPaint()
        : Paint("test-animated-username")
    {
    }

    QBrush asBrush(QColor, QRectF) const override
    {
        ++renders;
        return QBrush(frame == 1 ? Qt::red : Qt::green);
    }

    const std::vector<PaintDropShadow> &getDropShadows() const override
    {
        return this->shadows;
    }

    bool animated() const override
    {
        return this->frame.has_value();
    }

    std::optional<qint64> frame = 1;
    mutable int renders = 0;

protected:
    std::optional<qint64> frameCacheKey() const override
    {
        return this->frame;
    }

private:
    const std::vector<PaintDropShadow> shadows{{0, 2, 1, Qt::black}};
};

}  // namespace

TEST(SevenTVPaint, ReusesRenderedFramesAndSeparatesTheirAppearance)
{
    mock::BaseApplication app;
    app.settings.displaySevenTVPaintShadows = false;
    CountingPaint paint;
    const QFont font;
    const QSizeF size(96, 24);

    const auto first = paint.getPixmap("username", font, Qt::white, size, 1, 1);
    ASSERT_FALSE(first.isNull());
    const auto again = paint.getPixmap("username", font, Qt::white, size, 1, 1);
    EXPECT_EQ(again.cacheKey(), first.cacheKey());
    EXPECT_EQ(paint.renders, 1);

    paint.frame = 2;
    const auto next = paint.getPixmap("username", font, Qt::white, size, 1, 1);
    EXPECT_NE(next.toImage(), first.toImage());
    EXPECT_EQ(paint.renders, 2);

    paint.frame = 1;
    const auto loop = paint.getPixmap("username", font, Qt::white, size, 1, 1);
    EXPECT_EQ(loop.cacheKey(), first.cacheKey());

    const auto highDpi =
        paint.getPixmap("username", font, Qt::white, size, 1, 2);
    EXPECT_EQ(highDpi.devicePixelRatio(), 2);
    EXPECT_EQ(paint.renders, 3);

    app.settings.displaySevenTVPaintShadows = true;
    const auto shadow =
        paint.getPixmap("username", font, Qt::white, size, 1, 1);
    EXPECT_NE(shadow.cacheKey(), first.cacheKey());
    EXPECT_EQ(paint.renders, 4);

    paint.frame.reset();
    std::ignore = paint.getPixmap("username", font, Qt::white, size, 1, 1);
    EXPECT_EQ(paint.renders, 5);
}

}  // namespace chatterino
