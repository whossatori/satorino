// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/Notebook.hpp"

#include "util/NotebookTabLayout.hpp"
#include "widgets/helper/NotebookTab.hpp"

#include <algorithm>

namespace chatterino {

int Notebook::pinnedTabCount() const
{
    return static_cast<int>(std::count_if(
        this->items_.begin(), this->items_.end(), [](const Item &item) {
            return item.tab->isPinned();
        }));
}

void Notebook::updatePinnedTabOrder(QWidget *page)
{
    const int from = this->indexOf(page);
    if (from < 0)
    {
        return;
    }
    const int to = satorino::tabPinIndex(this->items_[from].tab->isPinned(),
                                         this->pinnedTabCount());
    this->items_.move(from, to);
    this->ensureSelectedTabVisible_ = true;
    this->performLayout(true);
    // The flag must be saved even when its index did not change. Layout locks
    // restrict user actions, not restoring pin state from a saved layout.
    this->afterPageMoved();
}

void Notebook::requestRemovePage(QWidget *page)
{
    auto item = this->findItem(page);
    if (item && !item->tab->isPinned())
    {
        this->removePage(page);
    }
}

}  // namespace chatterino
