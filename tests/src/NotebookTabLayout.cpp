// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "util/NotebookTabLayout.hpp"

#include <gtest/gtest.h>

using namespace chatterino::satorino;

TEST(NotebookTabLayout, ScrollClampsAtBothEdgesAndAfterResize)
{
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(-120, 1000, 300), 0);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(10000, 1000, 300), 700);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(123.5, 1000, 300), 123.5);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(700, 1000, 800), 200);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(700, 200, 300), 0);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(100, 0, 0), 0);
    EXPECT_DOUBLE_EQ(clampTabScrollOffset(100, 300, -1), 100);
}

TEST(NotebookTabLayout, SelectionRevealsEitherEdgeWithoutMovingVisibleTabs)
{
    EXPECT_DOUBLE_EQ(ensureTabVisible(300, 1000, 300, 100, 80), 100);
    EXPECT_DOUBLE_EQ(ensureTabVisible(0, 1000, 300, 400, 80), 180);
    EXPECT_DOUBLE_EQ(ensureTabVisible(100, 1000, 300, 120, 80), 100);
    EXPECT_DOUBLE_EQ(ensureTabVisible(0, 1000, 300, 920, 80), 700);
    EXPECT_DOUBLE_EQ(ensureTabVisible(600, 1000, 50, 400, 150), 400);
    EXPECT_DOUBLE_EQ(ensureTabVisible(600, 1000, 0, 400, 150), 600);
    EXPECT_DOUBLE_EQ(ensureTabVisible(600, 200, 300, 100, 80), 0);
}
