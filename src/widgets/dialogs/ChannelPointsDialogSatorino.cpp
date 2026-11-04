// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ChannelPointsDialog.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "providers/twitch/TwitchChannel.hpp"
#    include "singletons/Settings.hpp"
#    include "util/RedeemData.hpp"

#    include <QIcon>
#    include <QPushButton>
#    include <QToolButton>
#    include <QVBoxLayout>

#    include <algorithm>
#    include <utility>

namespace chatterino {
namespace {

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
    const auto roomId = this->channel_->roomId().trimmed();
    return roomId.isEmpty() ? this->rewardsChannelId_.trimmed() : roomId;
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
        const auto it =
            std::find_if(this->rewards_.begin(), this->rewards_.end(),
                         [this](const auto &r) {
                             return r.id == this->selectedReward_.id;
                         });
        if (it == this->rewards_.end())
        {
            this->selectedRewardValid_ = false;
            this->view_ = View::Rewards;
        }
        else
        {
            this->selectedReward_ = *it;
        }
    }
}

QVector<GqlChannelPointReward> ChannelPointsDialog::orderRedeems(
    QVector<GqlChannelPointReward> rewards)
{
    const auto data = redeemData();
    this->pinnedRedeemIds_.clear();
    for (const auto &reward : rewards)
    {
        if (data.isPinned(this->redeemChannelKey(), reward.id))
        {
            this->pinnedRedeemIds_.push_back(reward.id);
        }
    }
    return data.ordered(this->redeemChannelKey(), std::move(rewards));
}

void ChannelPointsDialog::addRedeemPin(QPushButton *card,
                                       const GqlChannelPointReward &reward)
{
    auto *pin = new QToolButton(card);
    const auto key = this->redeemChannelKey();
    const bool pinned = this->pinnedRedeemIds_.contains(reward.id);
    pin->setObjectName("SatorinoRedeemPin");
    pin->setCheckable(true);
    pin->setChecked(pinned);
    pin->setIcon(QIcon(pinned ? ":/buttons/pinEnabled.svg"
                              : ":/buttons/pinDisabled-darkMode.svg"));
    pin->setToolTip(pinned ? "Unpin reward" : "Pin reward to top");
    pin->setAccessibleName(pin->toolTip());
    pin->setCursor(Qt::PointingHandCursor);
    pin->setEnabled(!key.isEmpty() && !reward.id.isEmpty());
    const int size = std::max(18, int(22 * this->scale()));
    pin->setFixedSize(size, size);
    pin->setIconSize(QSize(size - 6, size - 6));
    pin->setStyleSheet("QToolButton { background: rgba(0,0,0,160); border: 0; "
                       "border-radius: 3px; }");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(pin, 0, Qt::AlignTop | Qt::AlignRight);
    layout->addStretch();
    // Pinning only changes a registered setting; it cannot spend points.
    QObject::connect(pin, &QToolButton::clicked, this,
                     [this, key, id = reward.id](bool checked) {
                         auto data = redeemData();
                         data.setPinned(key, id, checked,
                                        QDateTime::currentDateTime());
                         saveRedeemData(data);
                         QTimer::singleShot(0, this, [this] {
                             this->rebuildContent();
                         });
                     });
}

}  // namespace chatterino

#endif
