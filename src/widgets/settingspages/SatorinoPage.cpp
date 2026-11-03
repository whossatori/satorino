// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"

#include "singletons/Settings.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace chatterino {

SatorinoPage::SatorinoPage()
{
    auto *outer = new QVBoxLayout;
    auto *inner = new QHBoxLayout;
    auto *view = GeneralPageView::withNavigation(this);
    this->view_ = view;

    inner->addWidget(view);
    auto *frame = new QFrame;
    frame->setLayout(inner);
    outer->addWidget(frame);
    this->setLayout(outer);

    this->initLayout(*view);
}

bool SatorinoPage::filterElements(const QString &query)
{
    if (this->view_)
    {
        return this->view_->filterElements(query) || query.isEmpty();
    }

    return false;
}

void SatorinoPage::initLayout(GeneralPageView &layout)
{
    layout.addTitle("Tabs");
    SettingWidget::checkbox("Tab scroll", getSettings()->satorinoTabScroll)
        ->setTooltip("Shows all tabs in one row that you scroll with the mouse "
                     "wheel.")
        ->addTo(layout);
    layout.addStretch();
}

}  // namespace chatterino
