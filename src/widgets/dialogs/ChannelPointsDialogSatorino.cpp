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
#    include <QPointer>  // satorino: overlay lifetime tracking
#    include <QTimer>
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
        this->overlay_
            ->show();  // Child added to an already-visible preview needs this.
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
    return satorino::redeemChannelKey(this->channel_->roomId(),
                                      this->rewardsChannelId_);
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

void ChannelPointsDialog::addRedeemPin(QWidget *tile,
                                       const GqlChannelPointReward &reward,
                                       bool selected)
{
    if (!getSettings()->satorinoRedeemsEnablePinning.getValue())
    {
        return;
    }
    const auto key = this->redeemChannelKey();
    const auto id = satorino::redeemPinId(reward);
    const bool pinned = selected ? redeemData().isPinned(key, id)
                                 : this->pinnedRedeemIds_.contains(id);
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
    const auto unavailable =
        key.isEmpty() ? QString("Channel ID is not available yet; reopen "
                                "rewards after connecting.")
        : id.isEmpty()
            ? QString("This reward has no stable ID for pinning.")
            : QString{};  // Local pin edits do not depend on a pending redemption.
    pin->setEnabled(unavailable.isEmpty());
    if (!unavailable.isEmpty())
    {
        pin->setToolTip(unavailable);
        tile->setToolTip(unavailable);
    }
    // Parent preview has no click/redeem action; consume clicks in this child.
    pin->setAttribute(Qt::WA_NoMousePropagation);
    pin->setStyleSheet("QToolButton { background: rgba(0,0,0,160); border: 0; "
                       "border-radius: 3px; }");
    new RedeemPinAnchor(tile, pin, true);
    // Pinning only changes a registered setting; it cannot spend points.
    QObject::connect(
        pin, &QToolButton::clicked, this, [this, key, id](bool checked) {
            if (key != this->redeemChannelKey())
            {
                this->setStatus(
                    "Channel ID changed; reopen rewards before pinning.", true);
                return;
            }
            auto data = redeemData();
            data.setPinned(key, id, checked, QDateTime::currentDateTime());
            saveRedeemData(data);
            const bool saved = redeemData().isPinned(key, id);
            this->setStatus(
                saved     ? "Pinned"
                : checked ? "Could not save this pin (check the pin limit)."
                          : "Unpinned",
                checked && !saved);
            QTimer::singleShot(0, this, [this] {
                this->rebuildContent();
            });
        });
}

}  // namespace chatterino

#endif
