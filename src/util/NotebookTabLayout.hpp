// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

namespace chatterino::satorino {

// Coordinates are measured along the unscrolled strip, in widget pixels.
double clampTabScrollOffset(double offset, double contentWidth,
                            double viewportWidth);
double ensureTabVisible(double offset, double contentWidth,
                        double viewportWidth, double tabStart, double tabWidth);

// Indices are in the complete notebook order, including filtered-out tabs.
int tabInsertionIndex(int requested, int count, int pinnedCount);
int tabReorderIndex(int requested, bool pinned, int count, int pinnedCount);
// pinnedCount includes the tab's NEW pin state.
int tabPinIndex(bool pinned, int pinnedCount);

}  // namespace chatterino::satorino
