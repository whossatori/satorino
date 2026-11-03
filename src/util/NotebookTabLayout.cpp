// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "util/NotebookTabLayout.hpp"

#include <algorithm>

namespace chatterino::satorino {

double clampTabScrollOffset(double offset, double contentWidth,
                            double viewportWidth)
{
    return std::clamp(
        offset, 0.0,
        std::max(0.0, contentWidth - std::max(0.0, viewportWidth)));
}

double ensureTabVisible(double offset, double contentWidth,
                        double viewportWidth, double tabStart, double tabWidth)
{
    offset = clampTabScrollOffset(offset, contentWidth, viewportWidth);
    if (viewportWidth <= 0)
    {
        return offset;
    }

    // A tab wider than the viewport is aligned at its leading edge.
    if (tabStart < offset || tabWidth > viewportWidth)
    {
        offset = tabStart;
    }
    else if (tabStart + tabWidth > offset + viewportWidth)
    {
        offset = tabStart + tabWidth - viewportWidth;
    }
    return clampTabScrollOffset(offset, contentWidth, viewportWidth);
}

}  // namespace chatterino::satorino
