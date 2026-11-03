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

int tabInsertionIndex(int requested, int count, int pinnedCount)
{
    return std::clamp(requested < 0 ? count : requested, pinnedCount, count);
}

int tabReorderIndex(int requested, bool pinned, int count, int pinnedCount)
{
    if (count <= 0)
    {
        return -1;
    }
    return pinned ? std::clamp(requested, 0, pinnedCount - 1)
                  : std::clamp(requested, pinnedCount, count - 1);
}

int tabPinIndex(bool pinned, int pinnedCount)
{
    return std::max(0, pinnedCount - (pinned ? 1 : 0));
}

}  // namespace chatterino::satorino
