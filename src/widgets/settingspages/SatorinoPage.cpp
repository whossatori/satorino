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
    layout.addTitle("Appearance");
    SettingWidget::checkbox("Animated title paint",
                            getSettings()->satorinoAnimatedTitlePaint)
        ->setTooltip("Fill the main window title with the animated satorino "
                     "paint. Turning this off stops its animation timer.")
        ->addTo(layout);
    layout.addTitle("Channel point redeems");
    const QStringList keywords{"channel points", "rewards", "redeems",
                               "Twitch"};
    SettingWidget::checkbox("Show disabled redeems",
                            getSettings()->satorinoRedeemsShowDisabled)
        ->addKeywords(keywords)
        ->addTo(layout);
    SettingWidget::checkbox("Show paused redeems",
                            getSettings()->satorinoRedeemsShowPaused)
        ->addKeywords(keywords)
        ->addTo(layout);
    SettingWidget::checkbox("Show redeems only available during a stream when "
                            "the channel is offline",
                            getSettings()->satorinoRedeemsShowOfflineStream)
        ->setTooltip(
            "Include unavailable or cached rewards while offline. The existing "
            "Twitch data does not distinguish stream-only rewards from other "
            "offline unavailability. Cached rewards cannot be redeemed.")
        ->addKeywords(keywords)
        ->addKeywords({"live", "offline", "stream"})
        ->addTo(layout);
    SettingWidget::checkbox("Hide redeems on cooldown",
                            getSettings()->satorinoRedeemsHideCooldown)
        ->setTooltip("Hide live rewards reported as out of stock. The existing "
                     "Twitch data does not distinguish cooldown from other "
                     "stock limits or provide a remaining time.")
        ->addKeywords(keywords)
        ->addKeywords({"cooldown", "stock"})
        ->addTo(layout);
    SettingWidget::checkbox("Enable pinning redeems",
                            getSettings()->satorinoRedeemsEnablePinning)
        ->setTooltip("Select a reward to pin it using the small button inside "
                     "its preview. Turning pinning off keeps your saved pins.")
        ->addKeywords(keywords)
        ->addKeywords({"pin", "pinning"})
        ->addTo(layout);
    layout.addStretch();
}

}  // namespace chatterino
