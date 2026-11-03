// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/Notebook.hpp"

#include "singletons/Settings.hpp"
#include "util/NotebookTabLayout.hpp"
#include "widgets/buttons/DrawnButton.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Window.hpp"

#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace chatterino {

bool Notebook::usesTabScroll() const
{
    auto *window = dynamic_cast<Window *>(this->window());
    return window && window->getType() == WindowType::Main &&
           dynamic_cast<const SplitNotebook *>(this) &&
           this->getAllowUserTabManagement() && this->showTabs_ &&
           getSettings()->satorinoTabScroll &&
           (this->tabLocation_ == NotebookTabLocation::Top ||
            this->tabLocation_ == NotebookTabLocation::Bottom);
}

bool Notebook::updateTabStripParent(bool enabled)
{
    if (enabled && !this->tabStrip_)
    {
        // QWidget clips all child painting and input to this viewport.
        this->tabStrip_ = new QWidget(this);
        this->tabStrip_->setObjectName("satorinoTabStrip");
        this->tabStrip_->hide();
    }

    QWidget *parent = enabled ? this->tabStrip_ : this;
    bool changed = false;
    for (const auto &item : this->items_)
    {
        if (item.tab->parentWidget() != parent)
        {
            item.tab->setParent(parent);
            item.tab->setVisible(this->shouldShowTab(item.tab));
            changed = true;
        }
    }
    if (this->tabStrip_)
    {
        this->tabStrip_->setVisible(enabled && this->showTabs_);
    }
    if (changed)
    {
        this->ensureSelectedTabVisible_ = true;
    }
    if (!enabled)
    {
        this->tabScrollOffset_ = 0;
    }
    return changed;
}

void Notebook::performTabScrollLayout(const LayoutContext &ctx)
{
    const bool isBottom = this->tabLocation_ == NotebookTabLocation::Bottom;
    const int y = isBottom ? ctx.bottom - ctx.tabHeight - ctx.tabSpacer : 0;
    int left = ctx.left;
    int consumedButtonHeight = 0;
    for (auto *button : this->customButtons_)
    {
        if (button->isHidden())
        {
            continue;
        }
        button->setFixedSize(ctx.buttonWidth, ctx.buttonHeight);
        button->move(left, y);
        left += ctx.buttonWidth;
        consumedButtonHeight = ctx.tabHeight;
    }

    const int right = std::max(left, ctx.right - ctx.addButtonWidth);
    const int viewportWidth = right - left;
    int contentWidth = 0;
    int selectedStart = -1;
    int selectedWidth = 0;
    for (const auto &item : ctx.items)
    {
        if (item.page == this->selectedPage_)
        {
            selectedStart = contentWidth;
            selectedWidth = item.tab->minimumTabWidth();
        }
        contentWidth += item.tab->minimumTabWidth() + ctx.tabSpacer;
    }
    if (!ctx.items.empty())
    {
        contentWidth -= ctx.tabSpacer;
    }

    if (this->tabStrip_->width() != viewportWidth ||
        this->tabStripContentWidth_ != contentWidth)
    {
        this->ensureSelectedTabVisible_ = true;
    }
    this->tabStripContentWidth_ = contentWidth;
    this->tabScrollOffset_ = satorino::clampTabScrollOffset(
        this->tabScrollOffset_, contentWidth, viewportWidth);
    if (this->ensureSelectedTabVisible_ && selectedStart >= 0)
    {
        this->tabScrollOffset_ = satorino::ensureTabVisible(
            this->tabScrollOffset_, contentWidth, viewportWidth, selectedStart,
            selectedWidth);
    }
    this->ensureSelectedTabVisible_ = false;

    this->tabStrip_->setGeometry(left, y, viewportWidth, ctx.tabHeight);
    int x = -qRound(this->tabScrollOffset_);
    for (const auto &item : ctx.items)
    {
        item.tab->growWidth(0);
        item.tab->setInLastRow(true);
        item.tab->commitSize(false);
        // Immediate movement keeps high-resolution wheel input responsive and
        // cancels any old wrapping/reorder animation before changing parents.
        item.tab->queueMove(QPoint(x, 0), false);
        item.tab->commitMove();
        x += item.tab->minimumTabWidth() + ctx.tabSpacer;
    }
    if (this->showAddButton_)
    {
        this->addButton_->move(right, y);
    }

    const int consumed = this->showTabs_ ? ctx.tabHeight : 0;
    const int line =
        isBottom ? ctx.bottom -
                       std::max({consumed + ctx.tabSpacer, consumedButtonHeight,
                                 ctx.minimumTabAreaSpace}) -
                       ctx.lineThickness
                 : std::max({consumed, consumedButtonHeight,
                             ctx.minimumTabAreaSpace});
    if (this->lineOffset_ != line)
    {
        this->lineOffset_ = line;
        this->update();
    }
    if (this->selectedPage_)
    {
        const int pageTop = isBottom ? 0 : line + ctx.lineThickness;
        const int pageHeight = isBottom ? line : ctx.bottom - pageTop;
        this->selectedPage_->setGeometry(0, pageTop, this->width(),
                                         std::max(0, pageHeight));
        this->selectedPage_->raise();
    }
    this->tabStrip_->raise();
}

bool Notebook::scrollTabStrip(QWheelEvent *event)
{
    if (!this->usesTabScroll() || !this->showTabs_ || !this->tabStrip_ ||
        !this->tabStrip_->geometry().contains(
            this->mapFromGlobal(event->globalPosition().toPoint())))
    {
        return false;
    }

    // Even a row without overflow must not fall through to tab selection.
    event->accept();
    if (this->tabStripContentWidth_ <= this->tabStrip_->width())
    {
        return true;
    }

    const auto pixels = event->pixelDelta();
    const auto angles = event->angleDelta();
    const double delta =
        !pixels.isNull()
            ? (pixels.x() != 0 ? pixels.x() : pixels.y())
            : (angles.x() != 0 ? angles.x() : angles.y()) * this->scale() * 0.5;
    this->tabScrollOffset_ = satorino::clampTabScrollOffset(
        this->tabScrollOffset_ - delta, this->tabStripContentWidth_,
        this->tabStrip_->width());
    this->performLayout();
    return true;
}

}  // namespace chatterino
