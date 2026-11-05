// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/RedeemPin.hpp"
#include "Test.hpp"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QString>

#include <initializer_list>

using namespace chatterino::satorino;

TEST(RedeemPin, TooltipOnlySaysPinOrUnpinWhenAvailable)
{
    EXPECT_EQ(redeemPinTooltip(false, true, true), "Pin redeem");
    EXPECT_EQ(redeemPinTooltip(true, true, true), "Unpin redeem");
}
TEST(RedeemPin, TooltipExplainsEachDisabledOrFailedReason)
{
    EXPECT_EQ(redeemPinTooltip(false, false, true),
              "Can't pin: channel not loaded yet");
    EXPECT_EQ(redeemPinTooltip(false, true, false),
              "Can't pin: reward has no stable ID");
    EXPECT_EQ(redeemPinTooltip(false, false, false),
              "Can't pin: channel not loaded yet");
    EXPECT_EQ(redeemPinTooltip(false, true, true, true),
              "Can't pin: pin limit reached or settings unavailable");
}
TEST(RedeemPin, ChannelIdentityPrefersValidBroadcasterAndThenLegacyCatalog)
{
    EXPECT_EQ(redeemChannelKey(" 123 ", "456"), "123");
    EXPECT_EQ(redeemChannelKey("", "456"), "456");
    EXPECT_EQ(redeemChannelKey("not-a-broadcaster-id", "456"), "456");
    EXPECT_TRUE(redeemChannelKey("", "").isEmpty());
    EXPECT_TRUE(redeemChannelKey("0", "abc").isEmpty());
    EXPECT_TRUE(redeemChannelKey(QString(33, '1'), "abc").isEmpty());
}
TEST(RedeemPin, StarPlacementIsNineteenPercentWithFourPercentCornerMargin)
{
    for (qreal scale : {0.75, 1.0, 1.25, 1.5, 2.0})
        for (qreal size : {32.0, 64.0, 120.0})
        {
            const QRectF image(10, 20, size * scale, size * scale);
            const auto star = redeemStarRect(image);
            EXPECT_TRUE(image.contains(star));
            EXPECT_NEAR(star.width(), image.width() * 0.19, 1e-9);
            EXPECT_NEAR(image.right() - star.right(), image.width() * 0.04,
                        1e-9);
            EXPECT_NEAR(star.top() - image.top(), image.width() * 0.04, 1e-9);
        }
}
TEST(RedeemPin, StarPlacementHandlesEmptyOrShortImagesWithoutClipping)
{
    EXPECT_TRUE(redeemStarRect({}).isEmpty());
    const QRectF wide(20, 30, 100, 15);
    EXPECT_TRUE(wide.contains(redeemStarRect(wide)));
}
TEST(RedeemPin, PaintedStarIsYellowAndInsideItsImage)
{
    QImage image(120, 120, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintRedeemStar(painter, QRectF(10, 10, 100, 100));
    painter.end();
    bool yellow = false;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const auto colour = image.pixelColor(x, y);
            if (colour.alpha() == 0)
                continue;
            EXPECT_GE(x, 10);
            EXPECT_LT(x, 110);
            EXPECT_GE(y, 10);
            EXPECT_LT(y, 110);
            yellow = yellow || (colour.red() > 230 && colour.green() > 160 &&
                                colour.blue() < 100 && colour.alpha() > 200);
        }
    EXPECT_TRUE(yellow);
}

TEST(RedeemPin, PinFlagIsDormantWhenDisabled)
{
    EXPECT_FALSE(redeemPinVisible(false, false));
    EXPECT_FALSE(redeemPinVisible(false, true));
    EXPECT_FALSE(redeemPinVisible(true, false));
    EXPECT_TRUE(redeemPinVisible(true, true));
}
