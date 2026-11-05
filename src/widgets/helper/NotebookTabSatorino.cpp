// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTab.hpp"

#include "widgets/splits/SplitContainer.hpp"
#include "util/SatorinoIndicator.hpp"

#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace chatterino {

bool NotebookTab::isPinned() const
{
    return this->pinned_;
}

void NotebookTab::setPinned(bool value)
{
    if (this->pinned_ == value)
    {
        return;
    }
    this->pinned_ = value;
    this->mouseDownX_ = false;
    this->mouseOverX_ = false;
    this->refreshAndCommitSize(false);
    this->notebook_->updatePinnedTabOrder(this->page);
    this->update();
}

void NotebookTab::addPinAction()
{
    this->pinTabAction_ = this->menu_.addAction("Pin Tab", this, [this] {
        if (!this->notebook_->isNotebookLayoutLocked())
        {
            this->setPinned(!this->isPinned());
        }
    });
    QObject::connect(&this->menu_, &QMenu::aboutToShow, this,
                     &NotebookTab::updateTabManagementActions);
}

void NotebookTab::updateTabManagementActions()
{
    auto pinText = QString(this->isPinned() ? "Unpin Tab" : "Pin Tab");
    // Disabled menu tooltips vary by platform; keep the reason always visible.
    if (this->notebook_->isNotebookLayoutLocked())
    {
        pinText += " (turn off Lock Tab Layout in this menu)";
    }
    this->pinTabAction_->setText(pinText);
    this->pinTabAction_->setVisible(
        this->notebook_->getAllowUserTabManagement());
    this->pinTabAction_->setEnabled(!this->notebook_->isNotebookLayoutLocked());
    this->closeTabAction_->setEnabled(!this->isPinned());

    int all = 0;
    int others = 0;
    int before = 0;
    int after = 0;
    const int current = this->notebook_->indexOf(this->page);
    for (int i = 0; i < this->notebook_->getPageCount(); ++i)
    {
        auto *container =
            dynamic_cast<SplitContainer *>(this->notebook_->getPageAt(i));
        auto *tab = container ? container->getTab() : nullptr;
        if (!tab || !tab->isVisible() || tab->isPinned())
        {
            continue;
        }
        ++all;
        if (i != current)
        {
            ++others;
        }
        if (i < current)
        {
            ++before;
        }
        if (i > current)
        {
            ++after;
        }
    }
    this->closeMultipleTabsMenu_->setEnabled(all > 0);
    this->closeAllVisibleTabsAction_->setEnabled(all > 0);
    this->closeOtherVisibleTabsAction_->setEnabled(others > 0);
    this->closeTabsBeforeSelectedAction_->setEnabled(before > 0);
    this->closeTabsAfterSelectedAction_->setEnabled(after > 0);
}

void NotebookTab::paintPin(QPainter &painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(
        satorino::pinnedTabGlyphOrigin(this->getXRect(), this->scale()));
    const qreal pinScale = this->scale() * satorino::TAB_PIN_SCALE;
    painter.scale(pinScale, pinScale);
    auto pen = painter.pen();
    pen.setWidthF(1.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(pen.color());

    QPainterPath head;
    head.moveTo(-3, -5);
    head.lineTo(3, -5);
    head.lineTo(2, -3);
    head.lineTo(2, -1);
    head.lineTo(4, 1);
    head.lineTo(-4, 1);
    head.lineTo(-2, -1);
    head.lineTo(-2, -3);
    head.closeSubpath();
    painter.drawPath(head);
    painter.drawLine(QPointF(0, 1), QPointF(0, 6));
    painter.restore();
}

bool NotebookTab::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip && this->isPinned())
    {
        auto *help = static_cast<QHelpEvent *>(event);
        if (this->getXRect().contains(help->pos()))
        {
            QToolTip::showText(help->globalPos(),
                               "Pinned - right-click to unpin", this,
                               this->getXRect());
            event->accept();
            return true;
        }
    }
    return Button::event(event);
}

}  // namespace chatterino
