// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/SatorinoTitleLabel.hpp"

#include "Test.hpp"

using namespace chatterino;

TEST(SatorinoTitlePaint, EmbeddedWebpDecodesItsAnimationAndAuthoredDelays)
{
    const auto animation = loadSatorinoTitlePaint();
    ASSERT_EQ(animation.frames.size(), 150);
    EXPECT_EQ(animation.loops, -1);
    for (const auto &frame : animation.frames)
    {
        EXPECT_EQ(frame.image.size(), QSize(260, 64));
        EXPECT_EQ(frame.delay, 50);
    }
}
