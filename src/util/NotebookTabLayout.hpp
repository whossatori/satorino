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

}  // namespace chatterino::satorino
