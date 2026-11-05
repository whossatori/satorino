// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"

#include "singletons/Settings.hpp"
#include "Application.hpp"  // satorino: GUI status snapshot
#include "singletons/WindowManager.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QShowEvent>
#include <QHideEvent>
#include <QLabel>
#include <QColor>
#include <QPalette>
#include <QSizePolicy>
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
    this->raidStatusTimer_.setInterval(1000);
    QObject::connect(&this->raidStatusTimer_, &QTimer::timeout, this,
                     &SatorinoPage::refreshRaidStatus);
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
    // satorino: opt-in outgoing raid tabs, with one dependent switch option.
    layout.addTitle("Raids");
    SettingWidget::checkbox("Open a tab for the channel being raided",
                            getSettings()->satorinoRaidsOpenTab)
        ->setTooltip("Follow outgoing raids from up to ten open Twitch "
                     "channels. Twitch's shared EventSub budget may allow "
                     "fewer. Only opens chat; never sends a message.")
        ->addKeywords({"raid", "Twitch", "tab"})
        ->addTo(layout);
    SettingWidget::checkbox("Switch to the new tab",
                            getSettings()->satorinoRaidsSwitchTab)
        ->conditionallyEnabledBy(getSettings()->satorinoRaidsOpenTab)
        ->setTooltip(
            "Also selects an existing target tab. Switches only while the main "
            "window is active and its current input is empty.")
        ->addKeywords({"raid", "focus", "tab"})
        ->addTo(layout);
    // satorino: one calm text line; tooltip contains the full per-watch states.
    this->raidStatus_ = layout.addDescription("Raid watch: initializing");
    this->raidStatus_->setTextFormat(Qt::PlainText);
    this->raidStatus_->setOpenExternalLinks(false);
    this->raidStatus_->setWordWrap(false);
    this->raidStatus_->setSizePolicy(QSizePolicy::Ignored,
                                     QSizePolicy::Preferred);
    this->raidStatus_->setMinimumWidth(0);
    auto palette = this->raidStatus_->palette();
    palette.setColor(QPalette::WindowText, QColor("#888888"));
    this->raidStatus_->setPalette(palette);
    this->addLiveStatus(layout);
    layout.addStretch();
}

// satorino: snapshots are cheap and update only while this page is visible.
void SatorinoPage::showEvent(QShowEvent *event)
{
    SettingsPage::showEvent(event);
    this->refreshRaidStatus();
    this->raidStatusTimer_.start();
}
void SatorinoPage::hideEvent(QHideEvent *event)
{
    this->raidStatusTimer_.stop();
    SettingsPage::hideEvent(event);
}
void SatorinoPage::refreshRaidStatus()
{
    if (!this->raidStatus_ || !this->isVisible())
        return;
    this->raidStatus_->setText(getApp()->getWindows()->satorinoRaidStatus());
    this->raidStatus_->setToolTip(
        getApp()->getWindows()->satorinoRaidDetails());
}
}  // namespace chatterino
