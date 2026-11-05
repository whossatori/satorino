// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ChannelPointsDialog.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "providers/twitch/TwitchChannel.hpp"
#    include "singletons/Settings.hpp"
#    include "util/RedeemData.hpp"

#    include <QIcon>
#    include <QCursor>
#    include <QDateTime>
#    include <QSize>
#    include <QTimer>
#    include <QToolButton>
#    include <QVBoxLayout>

#    include <algorithm>
#    include <utility>

namespace chatterino {
namespace {

satorino::RedeemOptions redeemOptions()
{
    const auto *settings = getSettings();
    return {settings->satorinoRedeemsShowDisabled.getValue(),
            settings->satorinoRedeemsShowPaused.getValue(),
            settings->satorinoRedeemsShowOfflineStream.getValue(),
            settings->satorinoRedeemsHideCooldown.getValue(),
            settings->satorinoRedeemsEnablePinning.getValue()};
}

void sizePin(QToolButton *pin, float scale)
{
    const int size = std::max(18, static_cast<int>(18 * scale));
    pin->setFixedSize(size, size);
    pin->setIconSize(QSize(std::max(12, size - 4), std::max(12, size - 4)));
}

satorino::RedeemData redeemData()
{
    return satorino::RedeemData::fromJson(
        getSettings()->satorinoRedeemsState.getValue(),
        QDateTime::currentDateTime());
}

void saveRedeemData(const satorino::RedeemData &data)
{
    getSettings()->satorinoRedeemsState.setValue(data.toJson());
}

}  // namespace

QString ChannelPointsDialog::redeemChannelKey() const
{
    // Never use a display name: rooms retain preferences across renames.
    return satorino::redeemChannelKey(
        this->channel_->roomId(),
        satorino::redeemChannelKey(this->rewardsPinChannelId_,
                                   this->rewardsChannelId_));
}

void ChannelPointsDialog::loadCachedRedeems()
{
    this->rewards_ = redeemData().cached(this->redeemChannelKey());
    // A selected detail must not retain a redeemable copy while refreshing.
    if (this->selectedRewardValid_)
    {
        this->selectedReward_.isCached = true;
    }
}

void ChannelPointsDialog::mergeRedeems(const GqlChannelPointRewards &snapshot)
{
    auto data = redeemData();
    this->rewards_ = data.merge(
        this->redeemChannelKey(), snapshot.rewards, this->channel_->isLive(),
        snapshot.completeSnapshot, QDateTime::currentDateTime());
    saveRedeemData(data);
    if (this->selectedRewardValid_)
    {
        const auto it = std::find_if(
            this->rewards_.begin(), this->rewards_.end(),
            [this](const auto &r) {
                return satorino::redeemPinId(r) ==
                       satorino::redeemPinId(this->selectedReward_);
            });
        if (it == this->rewards_.end())
        {
            this->selectedRewardValid_ = false;
            this->view_ = View::Rewards;
        }
        else
        {
            this->selectedReward_ = *it;
            if (!satorino::redeemVisible(*it, this->channel_->isLive(),
                                         redeemOptions()))
            {
                this->selectedRewardValid_ = false;
                this->view_ = View::Rewards;
            }
        }
    }
}

void ChannelPointsDialog::bindRedeemOptions()
{
    auto refresh = [this](bool, auto) {
        QTimer::singleShot(0, this, [this] {
            if (this->selectedRewardValid_ &&
                !satorino::redeemVisible(this->selectedReward_,
                                         this->channel_->isLive(),
                                         redeemOptions()))
            {
                this->selectedRewardValid_ = false;
                this->view_ = View::Rewards;
            }
            this->rebuildContent();
        });
    };
    // The IRC room ID may arrive after an offline/cached detail was opened.
    this->managedConnections_.managedConnect(
        this->channel_->roomIdAssigned, [this] {
            QTimer::singleShot(0, this, [this] {
                this->rebuildContent();
            });
        });
    auto *settings = getSettings();
    settings->satorinoRedeemsShowDisabled.connect(
        refresh, this->managedConnections_, false);
    settings->satorinoRedeemsShowPaused.connect(
        refresh, this->managedConnections_, false);
    settings->satorinoRedeemsShowOfflineStream.connect(
        refresh, this->managedConnections_, false);
    settings->satorinoRedeemsHideCooldown.connect(
        refresh, this->managedConnections_, false);
    settings->satorinoRedeemsEnablePinning.connect(
        refresh, this->managedConnections_, false);
}

QVector<GqlChannelPointReward> ChannelPointsDialog::orderRedeems(
    QVector<GqlChannelPointReward> rewards)
{
    const auto options = redeemOptions();
    rewards.erase(std::remove_if(rewards.begin(), rewards.end(),
                                 [this, &options](const auto &r) {
                                     return !satorino::redeemVisible(
                                         r, this->channel_->isLive(), options);
                                 }),
                  rewards.end());
    const auto data = redeemData();
    this->pinnedRedeemIds_.clear();
    if (options.enablePinning)
    {
        for (const auto &reward : rewards)
        {
            if (data.isPinned(this->redeemChannelKey(),
                              satorino::redeemPinId(reward)))
            {
                this->pinnedRedeemIds_.push_back(satorino::redeemPinId(reward));
            }
        }
    }
    return data.displayOrder(this->redeemChannelKey(), std::move(rewards),
                             options.enablePinning);
}

bool ChannelPointsDialog::isRedeemPinned(
    const GqlChannelPointReward &reward) const
{
    return getSettings()->satorinoRedeemsEnablePinning.getValue() &&
           redeemData().isPinned(this->redeemChannelKey(),
                                 satorino::redeemPinId(reward));
}

void ChannelPointsDialog::addRedeemPinButton(
    const GqlChannelPointReward &reward, std::function<void(bool)> updateStar)
{
    if (!getSettings()->satorinoRedeemsEnablePinning.getValue())
        return;
    auto *pin = new QToolButton(this->contentWidget_);
    pin->setObjectName("SatorinoRedeemPin");
    pin->setCheckable(true);
    pin->setAutoRaise(true);
    pin->setMouseTracking(
        true);  // FramelessDraggable native hit test must return HTCLIENT.
    pin->setFocusPolicy(Qt::StrongFocus);
    sizePin(pin, this->scale());
    const auto key = this->redeemChannelKey();
    const auto id = satorino::redeemPinId(reward);
    const bool ready = !key.isEmpty() && !id.isEmpty() && id.size() <= 128;
    const auto refresh = [pin, updateStar](bool pinned, bool channelReady,
                                           bool rewardReady) {
        pin->setChecked(pinned);
        pin->setIcon(QIcon(pinned ? ":/buttons/pinEnabled.svg"
                                  : ":/buttons/pinDisabled-darkMode.svg"));
        pin->setEnabled(channelReady && rewardReady);
        pin->setToolTip(
            satorino::redeemPinTooltip(pinned, channelReady, rewardReady));
        pin->setAccessibleName(pin->toolTip());
        updateStar(pinned);
    };
    refresh(this->isRedeemPinned(reward), !key.isEmpty(),
            !id.isEmpty() && id.size() <= 128);
    pin->setCursor(ready ? Qt::PointingHandCursor : Qt::ArrowCursor);
    // Same parent/layout pattern as Redeem; this button never calls its action.
    this->contentLayout_->addWidget(pin, 0, Qt::AlignHCenter);
    QObject::connect(
        pin, &QToolButton::clicked, this,
        [this, pin, id, refresh](bool checked) {
            const auto currentKey = this->redeemChannelKey();
            const bool rewardReady = !id.isEmpty() && id.size() <= 128;
            if (currentKey.isEmpty() || !rewardReady)
            {
                refresh(false, !currentKey.isEmpty(), rewardReady);
                return;
            }
            auto data = redeemData();
            data.setPinned(currentKey, id, checked,
                           QDateTime::currentDateTime());
            saveRedeemData(data);
            const bool saved = redeemData().isPinned(currentKey, id);
            refresh(saved, true, true);
            if (saved != checked)
            {
                pin->setToolTip(
                    satorino::redeemPinTooltip(saved, true, true, true));
                pin->setAccessibleName(pin->toolTip());
            }
        });
}

void ChannelPointsDialog::refreshRedeemPinLayout()
{
    for (auto *pin :
         this->contentWidget_->findChildren<QToolButton *>("SatorinoRedeemPin"))
        sizePin(pin, this->scale());
}

}  // namespace chatterino

#endif
