// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/raids/SatorinoRaidController.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/QLogging.hpp"
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
#include <QJsonObject>
#include <QLoggingCategory>
#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <algorithm>
#include <utility>

namespace chatterino::satorino {
namespace {
QString sourceId(const std::shared_ptr<TwitchChannel> &channel,
                 const std::shared_ptr<TwitchAccount> &account)
{
    if (validRaidId(channel->roomId()))
        return channel->roomId();
    // Own channel ID is already authoritative even before the first ROOMSTATE.
    if (account &&
        channel->getName().compare(account->getUserName(),
                                   Qt::CaseInsensitive) == 0 &&
        validRaidId(account->getUserId()))
        return account->getUserId();
    return {};
}
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
    getSettings()->enableExperimentalEventSub.connect(
        [this](bool) {
            if (getSettings()->satorinoRaidsOpenTab.getValue())
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        this->publishStatus();
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
    this->token_.clear();
    this->client_.clear();
    this->waitingForIds_ = false;
    this->enabledAtUtcMs_ = QDateTime::currentMSecsSinceEpoch();
}
void RaidController::configure()
{
    assertInGuiThread();
    this->stop();
    this->accountConnections_.clear();
    this->accountChangedConnection_.disconnect();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
    {
        this->publishStatus();
        return;
    }
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
    {
        this->publishStatus();
        return;
    }
    this->ownerId_ = account->getUserId();
    this->token_ = account->getOAuthToken();
    this->client_ = account->getOAuthClient();
    if (!getApp()->getEventSub()->raidsAvailable())
    {
        this->publishStatus();
        return;
    }
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
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon() ||
        account->getUserId() != this->ownerId_ ||
        account->getOAuthToken() != this->token_ ||
        account->getOAuthClient() != this->client_)
    {
        this->accountChanged();
        return;
    }
    auto *eventSub = getApp()->getEventSub();
    if (!eventSub->raidsAvailable())
        return;
    std::map<QString, QString> ids;
    this->waitingForIds_ = false;
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  const auto id = sourceId(channel, account);
                  if (!id.isEmpty())
                      ids.emplace(id, channel->getName());
                  else
                      this->waitingForIds_ = true;
              });
    std::erase_if(this->watches_, [&ids](const auto &entry) {
        return !ids.contains(entry.first);
    });
    for (const auto &[id, login] : ids)
    {
        if (this->watches_.contains(id))
            continue;
        if (this->watches_.size() >= RAID_WATCH_LIMIT)
            break;
        Watch watch;
        watch.login = login;
        watch.request.subscriptionType = "channel.raid";
        watch.request.subscriptionVersion = "1";
        watch.request.ownerTwitchUserID = this->ownerId_;
        watch.request.conditions.emplace_back("from_broadcaster_user_id", id);
        this->watches_.emplace(id, std::move(watch));
    }
    const auto now = this->clock_.elapsed();
    for (auto &[id, watch] : this->watches_)
    {
        if (watch.handle)
        {
            auto status = eventSub->raidSubscriptionStatus(watch.request);
            if ((status.state == RaidWatchState::Waiting ||
                 status.state == RaidWatchState::Subscribing) &&
                now - watch.startedAt >= 90000)
                status = {RaidWatchState::Failed,
                          "connection/subscription timed out"};
            if (status.state == RaidWatchState::Failed ||
                status.state == RaidWatchState::Inactive)
            {
                watch.last = {RaidWatchState::Failed,
                              status.detail.isEmpty()
                                  ? QString("subscription lost")
                                  : status.detail};
                watch.handle.reset();
                watch.retryAt = now + raidRetrySeconds(watch.failures) * 1000;
                watch.failures = std::min(4, watch.failures + 1);
            }
            else
            {
                watch.last = status;
                if (status.state == RaidWatchState::Subscribed)
                    watch.failures = 0;
            }
        }
        if (!watch.handle && now >= watch.retryAt)
        {
            qCInfo(chatterinoTwitchEventSub)
                << "Raid watch subscribing" << watch.login << id;
            watch.handle = eventSub->subscribe(watch.request);
            watch.startedAt = now;
            watch.retryAt = 0;
            if (watch.handle)
                watch.last = eventSub->raidSubscriptionStatus(watch.request);
            else
            {
                watch.last = {RaidWatchState::Failed,
                              "EventSub returned no subscription handle"};
                watch.retryAt = now + raidRetrySeconds(watch.failures) * 1000;
                watch.failures = std::min(4, watch.failures + 1);
            }
        }
    }
    this->publishStatus();
}
RaidStatusContext RaidController::statusContext() const
{
    assertInGuiThread();
    RaidStatusContext status;
    status.enabled = getSettings()->satorinoRaidsOpenTab.getValue();
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    status.loggedIn = account && !account->isAnon();
    status.hasToken = status.loggedIn && !account->getOAuthToken().isEmpty();
    status.eventSubAvailable = getApp()->getEventSub()->raidsAvailable();
    status.eventSubSetting =
        getSettings()->enableExperimentalEventSub.getValue();
    status.waitingForIds = this->waitingForIds_;
    status.lastEvent = this->lastEvent_;
    const auto now = this->clock_.elapsed();
    for (const auto &[id, watch] : this->watches_)
        status.watches.push_back(
            {watch.login, watch.last,
             watch.retryAt > now
                 ? static_cast<int>((watch.retryAt - now + 999) / 1000)
                 : 0});
    return status;
}
void RaidController::publishStatus()
{
    const auto status = this->statusContext();
    // Retry countdown is UI-only; log state changes without a line every second.
    auto transitions = status;
    for (auto &watch : transitions.watches)
        watch.retrySeconds = 0;
    const auto text =
        raidStatusText(transitions) + "\n" + raidStatusDetails(transitions);
    if (text == this->lastStatus_)
        return;
    this->lastStatus_ = text;
    if (std::any_of(status.watches.begin(), status.watches.end(),
                    [](const auto &watch) {
                        return watch.subscription.state ==
                               RaidWatchState::Failed;
                    }))
        qCWarning(chatterinoTwitchEventSub).noquote()
            << raidStatusText(transitions);
    else
        qCInfo(chatterinoTwitchEventSub).noquote()
            << raidStatusText(transitions);
}
QString RaidController::receive(const QJsonObject &payload, qint64 timestampMs,
                                const QString &ownerId, qint64 receivedAtMs)
{
    return this->receiveImpl(payload, timestampMs, ownerId, receivedAtMs,
                             false);
}

QString RaidController::receiveImpl(const QJsonObject &payload,
                                    qint64 timestampMs, const QString &ownerId,
                                    qint64 receivedAtMs, bool synthetic)
{
    assertInGuiThread();
    QString parseError;
    const auto event = parseRaidEvent(payload, timestampMs, &parseError);
    const auto finish = [&](const QString &result) {
        QString route = "unknown source -> target";
        if (event)
        {
            const auto watched = this->watches_.find(event->sourceId);
            route = (watched != this->watches_.end() ? watched->second.login
                                                     : event->sourceId) +
                    " -> " + event->targetLogin;
        }
        this->lastEvent_ =
            QString("Last raid event: %1 at %2, %3%4")
                .arg(route, QDateTime::currentDateTime().toString("HH:mm"),
                     synthetic ? "test: " : "", result);
        qCInfo(chatterinoTwitchEventSub).noquote() << this->lastEvent_;
        this->publishStatus();
        return this->lastEvent_;
    };
    if (isAppAboutToQuit())
        return finish("ignored (application shutting down)");
    if (!getSettings()->satorinoRaidsOpenTab.getValue())
        return finish("ignored (setting off)");
    if (!event)
        return finish("ignored (" + parseError + ")");
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon())
        return finish("ignored (not logged in)");
    if (account->getOAuthToken().isEmpty())
        return finish("ignored (account has no OAuth token)");
    if (ownerId != this->ownerId_ || account->getUserId() != this->ownerId_)
        return finish(
            "ignored (connection/account owner changed or unavailable)");
    const auto watch = this->watches_.find(event->sourceId);
    if (!synthetic && (watch == this->watches_.end() || !watch->second.handle))
        return finish("ignored (source has no active subscription request)");
    QPointer<Window> sourceWindow;
    QPointer<Window> targetWindow;
    QPointer<SplitContainer> targetPage;
    QPointer<Split> targetSplit;
    visitOpen(this->windows_, [&](Window *window, SplitContainer *page,
                                  Split *split, const auto &channel) {
        if (sourceId(channel, account) == event->sourceId && !sourceWindow)
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
    context.receivedAtUtcMs = receivedAtMs;
    const auto decision = this->policy_.decide(*event, context);
    if (decision.action == RaidAction::Ignore)
        return finish("ignored (" + decision.reason + ")");
    if (decision.action == RaidAction::OpenAndJoin)
    {
        const auto channel =
            getApp()->getTwitch()->getOrAddChannel(event->targetLogin);
        const auto current = getApp()->getAccounts()->twitch.getCurrent();
        if (!sourceWindow || !channel || channel->isEmpty() ||
            channel->getType() != Channel::Type::Twitch ||
            !getSettings()->satorinoRaidsOpenTab.getValue() || !current ||
            current->isAnon() || current->getUserId() != ownerId)
            return finish("ignored (channel creation failed, source closed or "
                          "account/setting changed)");
        // appendNewSplit requests focus internally; restore the prior widget when not switching.
        const QPointer<QWidget> focus = QApplication::focusWidget();
        targetWindow = sourceWindow;
        targetPage = sourceWindow->getNotebook().addPage(false);
        targetSplit = targetPage->appendNewSplit(false);
        targetSplit->setChannel(IndirectChannel(channel));
        if (!decision.switchToTab && focus)
            focus->setFocus(Qt::OtherFocusReason);
    }
    bool switched = false;
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
        switched = true;
    }
    const auto action = decision.action == RaidAction::OpenAndJoin
                            ? QString("tab opened")
                            : QString("existing tab reused");
    return finish(action + (switched
                                ? "; switched"
                                : "; not switched (" +
                                      (decision.reason.isEmpty()
                                           ? QString("focus/settings changed")
                                           : decision.reason) +
                                      ")"));
}
QString RaidController::simulate(const ChannelPtr &source,
                                 const QString &targetLogin,
                                 const QString &targetId)
{
    assertInGuiThread();
    if (!getSettings()->satorinoRaidsOpenTab.getValue())
        return "Raid test ignored: setting off";
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    const auto channel = std::dynamic_pointer_cast<TwitchChannel>(source);
    if (!channel || source->getType() != Channel::Type::Twitch)
        return "Raid test requires the current real Twitch channel";
    if (!account || account->isAnon())
        return "Raid test ignored: not logged in";
    QString id = targetId;
    const auto login = targetLogin.toLower();
    // Offline lookup only; no Helix request or fabricated broadcaster ID.
    const auto known = std::dynamic_pointer_cast<TwitchChannel>(
        getApp()->getTwitch()->getChannelOrEmpty(login));
    if (known && !known->roomId().isEmpty() && !id.isEmpty() &&
        id != known->roomId())
        return "Raid test ignored: supplied target ID does not match the "
               "locally known channel";
    if (id.isEmpty() && known)
        id = known->roomId();
    if (!validRaidId(id) || !validRaidLogin(login))
        return "Raid test: target ID unavailable locally. Use "
               "/satorino-testraid <targetLogin> <targetId>; no lookup is sent "
               "to Twitch.";
    const auto sourceID = sourceId(channel, account);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const QJsonObject payload{
        {"subscription",
         QJsonObject{{"type", "channel.raid"},
                     {"version", "1"},
                     {"condition",
                      QJsonObject{{"from_broadcaster_user_id", sourceID}}}}},
        {"event", QJsonObject{{"from_broadcaster_user_id", sourceID},
                              {"to_broadcaster_user_id", id},
                              {"to_broadcaster_user_login", login},
                              {"to_broadcaster_user_name", login}}}};
    // Only the transport membership check is synthetic; all parser/policy/action guards remain.
    return this->receiveImpl(payload, now, account->getUserId(), now, true);
}
}  // namespace chatterino::satorino
