// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ChannelPointsDialog.hpp"

#if MOLTORINO_ENABLE_CHANNEL_POINT_REWARDS

#    include "providers/twitch/TwitchChannel.hpp"
#    include "singletons/Settings.hpp"
#    include "util/RedeemData.hpp"

#    include <QEvent>
#    include <QIcon>
#    include <QLabel>
#    include <QToolButton>

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

// An overlay has no layout item and cannot affect a tile's size hints.
class RedeemPinAnchor : public QObject
{
public:
    RedeemPinAnchor(QWidget *tile, QWidget *overlay, bool selected)
        : QObject(tile)
        , tile_(tile)
        , overlay_(overlay)
        , selected_(selected)
    {
        tile->installEventFilter(this);
        this->position();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
        {
            this->position();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void position()
    {
        if (!this->tile_ || !this->overlay_)
        {
            return;
        }
        const int size = this->selected_
                             ? std::max(12, this->tile_->width() / 4)
                             : std::max(8, this->tile_->width() / 10);
        this->overlay_->setFixedSize(size, size);
        if (auto *button = qobject_cast<QToolButton *>(this->overlay_.data()))
        {
            button->setIconSize(QSize(size - 4, size - 4));
        }
        this->overlay_->move(std::max(0, this->tile_->width() - size - 2), 2);
        this->overlay_->raise();
    }

    QPointer<QWidget> tile_;
    QPointer<QWidget> overlay_;
    bool selected_;
};

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
            if (data.isPinned(this->redeemChannelKey(), reward.id))
            {
                this->pinnedRedeemIds_.push_back(reward.id);
            }
        }
    }
    return data.ordered(this->redeemChannelKey(), std::move(rewards),
                        options.enablePinning);
}

void ChannelPointsDialog::addRedeemPin(QWidget *tile,
                                       const GqlChannelPointReward &reward,
                                       bool selected)
{
    if (!getSettings()->satorinoRedeemsEnablePinning.getValue())
    {
        return;
    }
    const auto key = this->redeemChannelKey();
    const bool pinned = selected ? redeemData().isPinned(key, reward.id)
                                 : this->pinnedRedeemIds_.contains(reward.id);
    if (!selected)
    {
        if (pinned)
        {
            auto *mark = new QLabel(tile);
            mark->setObjectName("SatorinoRedeemPinMark");
            mark->setPixmap(QIcon(":/buttons/pinEnabled.svg").pixmap(32, 32));
            mark->setScaledContents(true);
            mark->setAttribute(Qt::WA_TransparentForMouseEvents);
            mark->setFocusPolicy(Qt::NoFocus);
            tile->setToolTip(tile->toolTip() + "\nPinned");
            new RedeemPinAnchor(tile, mark, false);
        }
        return;
    }
    // Only the selected detail's preview gets an interactive corner button.
    auto *pin = new QToolButton(tile);
    pin->setObjectName("SatorinoRedeemPin");
    pin->setCheckable(true);
    pin->setChecked(pinned);
    pin->setIcon(QIcon(pinned ? ":/buttons/pinEnabled.svg"
                              : ":/buttons/pinDisabled-darkMode.svg"));
    pin->setToolTip(pinned ? "Unpin reward" : "Pin reward to top");
    pin->setAccessibleName(pin->toolTip());
    pin->setCursor(Qt::PointingHandCursor);
    pin->setEnabled(!key.isEmpty() && !reward.id.isEmpty() &&
                    !this->actionInFlight_);
    pin->setStyleSheet("QToolButton { background: rgba(0,0,0,160); border: 0; "
                       "border-radius: 3px; }");
    new RedeemPinAnchor(tile, pin, true);
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
