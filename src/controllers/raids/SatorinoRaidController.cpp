// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/raids/SatorinoRaidController.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/eventsub/Controller.hpp"
#include "providers/twitch/eventsub/SubscriptionRequest.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchAccountManager.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/MultiChannel.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QApplication>
#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <utility>

namespace chatterino::satorino {
namespace {
using Visitor = std::function<void(Window *, SplitContainer *, Split *,
                                   const std::shared_ptr<TwitchChannel> &)>;
void visitOpen(WindowManager &windows, const Visitor &visit)
{
    for (auto *window : windows.windows())
    {
        auto &notebook = window->getNotebook();
        // Public count includes hidden tabs; do not use Notebook's protected items().
        for (int i = 0; i < notebook.getPageCount(); ++i)
        {
            auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
            if (!page)
                continue;
            for (auto *split : page->getSplits())
            {
                const auto collect = [&](const ChannelPtr &channel) {
                    if (channel && channel->getType() == Channel::Type::Twitch)
                        if (auto twitch =
                                std::dynamic_pointer_cast<TwitchChannel>(
                                    channel))
                            visit(window, page, split, twitch);
                };
                const auto channel = split->getChannel();
                if (const auto multi =
                        std::dynamic_pointer_cast<MultiChannel>(channel))
                    for (const auto &child : multi->channels())
                        collect(child.channel);
                else
                    collect(channel);
            }
        }
    }
}
struct InputState {
    bool mainActive = false;
    bool hasDraft = false;
};
InputState currentInput(WindowManager &windows)
{
    Window *main = nullptr;
    for (auto *window : windows.windows())
        if (window->getType() == WindowType::Main)
            main = window;
    if (!main)
        return {};
    auto *page = main->getNotebook().getSelectedPage();
    auto *split = page ? page->getSelectedSplit() : nullptr;
    return {main->isActiveWindow(),
            split && !split->getInput().getInputText().isEmpty()};
}
bool canSwitch(WindowManager &windows)
{
    const auto input = currentInput(windows);
    return input.mainActive && !input.hasDraft;
}
}  // namespace
RaidController::RaidController(WindowManager &windows)
    : QObject(&windows)
    , windows_(windows)
{
    this->clock_.start();
    this->timer_.setInterval(1000);
    QObject::connect(&this->timer_, &QTimer::timeout, this,
                     &RaidController::sync);
    getSettings()->satorinoRaidsOpenTab.connect(
        [this](bool) {
            QMetaObject::invokeMethod(
                this,
                [this] {
                    this->configure();
                },
                Qt::AutoConnection);
        },
        this->settingsConnections_, false);
    this->configure();
}
void RaidController::stop()
{
    assertInGuiThread();
    this->timer_.stop();
    this->watches_.clear();
    this->ownerId_.clear();
    this->enabledAtUtcMs_ = QDateTime::currentMSecsSinceEpoch();
}
void RaidController::configure()
{
    assertInGuiThread();
    this->stop();
    this->accountConnections_.clear();
    this->accountChangedConnection_.disconnect();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
        return;
    auto &accounts = getApp()->getAccounts()->twitch;
    this->accountConnections_.managedConnect(
        accounts.currentUserAboutToChange,
        [this](const std::shared_ptr<TwitchAccount> &,
               const std::shared_ptr<TwitchAccount> &) {
            // Release while Helix still has the old account's credentials.
            this->stop();
        });
    this->accountChangedConnection_ =
        accounts.currentUserChanged.connect([this] {
            this->accountChanged();
        });
    this->accountChanged();
}
void RaidController::accountChanged()
{
    assertInGuiThread();
    this->stop();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
        return;
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon() || account->getOAuthToken().isEmpty())
        return;
    this->ownerId_ = account->getUserId();
    this->sync();
    this->timer_.start();
}
void RaidController::sync()
{
    assertInGuiThread();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
    {
        this->stop();
        return;
    }
    std::set<QString> ids;
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  if (validRaidId(channel->roomId()))
                      ids.insert(channel->roomId());
              });
    // Preserve existing watched IDs when the bounded set is full, avoiding churn.
    std::erase_if(this->watches_, [&ids](const auto &entry) {
        return !ids.contains(entry.first);
    });
    for (const auto &id : ids)
    {
        if (this->watches_.size() >= RAID_WATCH_LIMIT)
            break;
        if (this->watches_.contains(id))
            continue;
        eventsub::SubscriptionRequest request;
        request.subscriptionType = "channel.raid";
        request.subscriptionVersion = "1";
        request.ownerTwitchUserID = this->ownerId_;
        request.conditions.emplace_back("from_broadcaster_user_id", id);
        this->watches_.emplace(id, getApp()->getEventSub()->subscribe(request));
    }
}
void RaidController::receive(const QJsonObject &payload, qint64 timestampMs,
                             const QString &ownerId)
{
    assertInGuiThread();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit() ||
        ownerId != this->ownerId_)
        return;
    const auto event = parseRaidEvent(payload, timestampMs);
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!event || !account || account->isAnon() ||
        account->getUserId() != this->ownerId_ ||
        !this->watches_.contains(event->sourceId))
        return;
    QPointer<Window> sourceWindow;
    QPointer<Window> targetWindow;
    QPointer<SplitContainer> targetPage;
    QPointer<Split> targetSplit;
    visitOpen(this->windows_, [&](Window *window, SplitContainer *page,
                                  Split *split, const auto &channel) {
        if (channel->roomId() == event->sourceId && !sourceWindow)
            sourceWindow = window;
        if (channel->roomId() == event->targetId ||
            channel->getName().compare(event->targetLogin,
                                       Qt::CaseInsensitive) == 0)
        {
            targetWindow = window;
            targetPage = page;
            targetSplit = split;
        }
    });
    RaidContext context;
    context.enabled = getSettings()->satorinoRaidsOpenTab.getValue();
    context.switchToTab = getSettings()->satorinoRaidsSwitchTab.getValue();
    context.targetExists = targetPage != nullptr;
    context.targetBlocked =
        account->blockedUserIds().contains(event->targetId) ||
        account->blockedUserLogins().contains(event->targetLogin);
    context.loggedIn = !account->isAnon();
    context.sourceOpen = sourceWindow != nullptr;
    const auto input = currentInput(this->windows_);
    context.mainActive = input.mainActive;
    context.hasDraft = input.hasDraft;
    context.nowMs = this->clock_.elapsed();
    context.nowUtcMs = QDateTime::currentMSecsSinceEpoch();
    context.enabledAtUtcMs = this->enabledAtUtcMs_;
    const auto decision = this->policy_.decide(*event, context);
    if (decision.action == RaidAction::Ignore)
        return;
    if (decision.action == RaidAction::OpenAndJoin)
    {
        const auto channel =
            getApp()->getTwitch()->getOrAddChannel(event->targetLogin);
        const auto current = getApp()->getAccounts()->twitch.getCurrent();
        if (!sourceWindow || !channel || channel->isEmpty() ||
            channel->getType() != Channel::Type::Twitch ||
            !getSettings()->satorinoRaidsOpenTab.getValue() || !current ||
            current->isAnon() || current->getUserId() != ownerId)
            return;
        // appendNewSplit requests focus internally; restore the prior widget when not switching.
        const QPointer<QWidget> focus = QApplication::focusWidget();
        targetWindow = sourceWindow;
        targetPage = sourceWindow->getNotebook().addPage(false);
        targetSplit = targetPage->appendNewSplit(false);
        targetSplit->setChannel(IndirectChannel(channel));
        if (!decision.switchToTab && focus)
            focus->setFocus(Qt::OtherFocusReason);
    }
    // Recheck immediately before selection; never activate an external/background window.
    if (decision.switchToTab &&
        getSettings()->satorinoRaidsOpenTab.getValue() &&
        getSettings()->satorinoRaidsSwitchTab.getValue() &&
        canSwitch(this->windows_) && targetWindow && targetPage && targetSplit)
    {
        // An aggregate child also counts as an existing target. Only change its
        // active input channel when the same focus/draft guard permits switching.
        if (const auto multi = std::dynamic_pointer_cast<MultiChannel>(
                targetSplit->getChannel()))
        {
            const auto children = multi->channels();
            for (std::size_t i = 0; i < children.size(); ++i)
                if (children[i].channel->getName().compare(
                        event->targetLogin, Qt::CaseInsensitive) == 0)
                {
                    multi->setActiveChannelIndex(i);
                    break;
                }
        }
        targetWindow->getNotebook().select(targetPage);
        targetSplit->setFocus(Qt::OtherFocusReason);
        targetWindow->raise();
        targetWindow->activateWindow();
    }
}
}  // namespace chatterino::satorino
