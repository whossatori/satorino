// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/SatorinoPage.hpp"

#include "controllers/emotes/EmotePriority.hpp"
#include "singletons/Settings.hpp"
#include "Application.hpp"  // satorino: GUI status snapshot
#include "singletons/WindowManager.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QShowEvent>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QColor>
#include <QPalette>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <array>
#include <cstddef>
#include <utility>

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
    layout.addTitle("Updates");
    SettingWidget::checkbox("Automatically update on launch",
                            getSettings()->satorinoAutoUpdate)
        ->setTooltip(
            "Check for Satorino releases when starting an installed build "
            "or an AppImage.")
        ->addTo(layout);
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
    this->addEmotePriority(layout);
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
        ->setTooltip("Follow outgoing raids from open Twitch channels using "
                     "the shared raid status feed, even when the raid bar is "
                     "hidden. Only opens chat; never sends a message.")
        ->addKeywords({"raid", "Twitch", "tab"})
        ->addTo(layout);
    SettingWidget::checkbox("Switch to the new tab",
                            getSettings()->satorinoRaidsSwitchTab)
        ->conditionallyEnabledBy(getSettings()->satorinoRaidsOpenTab)
        ->setTooltip(
            "Selects the target unless the current input has an unsent draft. "
            "While another app is active, selects silently without raising or "
            "focusing satorino.")
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
    this->addLocalFlags(layout);
    this->addLiveStatus(layout);
    layout.addStretch();
}

void SatorinoPage::addEmotePriority(GeneralPageView &layout)
{
    layout.addTitle("Emote priority");
    layout.addDescription(
        "When providers share an emote name, the highest provider with that "
        "emote wins in new chat messages.");

    auto &setting = getSettings()->satorinoEmotePriority;
    auto *group = new QWidget;
    auto *rowsLayout = new QVBoxLayout(group);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);

    struct ProviderRow {
        QLabel *name;
        QPushButton *up;
        QPushButton *down;
    };
    std::array<ProviderRow, DEFAULT_EMOTE_PRIORITY.size()> rows{};
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        auto *row = new QWidget(group);
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(2, 4, 2, 4);
        line->setSpacing(4);
        auto &controls = rows[i];
        controls.name = new QLabel(row);
        controls.up = new QPushButton(QStringLiteral("↑"), row);
        controls.down = new QPushButton(QStringLiteral("↓"), row);
        line->addWidget(controls.name, 1);
        line->addWidget(controls.up);
        line->addWidget(controls.down);
        controls.up->setFixedWidth(26);
        controls.down->setFixedWidth(26);
        controls.up->setEnabled(i > 0);
        controls.down->setEnabled(i + 1 < rows.size());
        rowsLayout->addWidget(row);

        const auto move = [&setting, i](std::size_t target) {
            auto order = parseEmotePriority(setting.getValue());
            if (target >= order.size())
            {
                return;
            }
            std::swap(order[i], order[target]);
            QStringList saved;
            for (const auto provider : order)
            {
                saved.append(emoteProviderName(provider).toString());
            }
            setting.setValue(saved);
        };
        if (i > 0)
        {
            QObject::connect(controls.up, &QPushButton::clicked, row,
                             [move, i] {
                                 move(i - 1);
                             });
        }
        if (i + 1 < rows.size())
        {
            QObject::connect(controls.down, &QPushButton::clicked, row,
                             [move, i] {
                                 move(i + 1);
                             });
        }
    }

    auto *reset = new QPushButton("Reset order", group);
    rowsLayout->addWidget(reset, 0, Qt::AlignRight);
    QObject::connect(reset, &QPushButton::clicked, group, [&setting] {
        setting.setValue(QStringList{});
    });

    setting.connect(
        [rows, reset](const QStringList &saved) {
            const auto order = parseEmotePriority(saved);
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const auto name = emoteProviderName(order[i]).toString();
                const auto &controls = rows[i];
                controls.name->setText(name);
                controls.up->setToolTip(QStringLiteral("Move %1 up").arg(name));
                controls.down->setToolTip(
                    QStringLiteral("Move %1 down").arg(name));
                controls.up->setAccessibleName(controls.up->toolTip());
                controls.down->setAccessibleName(controls.down->toolTip());
            }
            reset->setEnabled(order != DEFAULT_EMOTE_PRIORITY);
        },
        this->managedConnections_);

    layout.addWidget(group, {"Emote priority", "Twitch", "7TV", "FFZ", "BTTV"});
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
