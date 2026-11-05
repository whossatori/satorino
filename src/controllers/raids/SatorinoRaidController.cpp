// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/raids/SatorinoRaidController.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/PubSubManager.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchAccountManager.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/MultiChannel.hpp"
#include "util/PostToThread.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/SplitInput.hpp"
#include "widgets/Window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

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
    this->deadlineTimer_.setSingleShot(true);
    QObject::connect(&this->deadlineTimer_, &QTimer::timeout, this,
                     &RaidController::deadlineExpired);
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
    ++this->generation_;
    this->deadlineTimer_.stop();
    this->feedConnection_.clear();
    this->feed_ = RaidFeedTracker{};
    this->ownerId_.clear();
    this->enabledAtUtcMs_ = QDateTime::currentMSecsSinceEpoch();
}
void RaidController::configure()
{
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
    this->stop();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
        return;
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon() || account->getOAuthToken().isEmpty())
        return;
    this->ownerId_ = account->getUserId();
    // One shared feed connection. The callback retains no controller/window pointer across threads.
    this->feedConnection_.managedConnect(
        getApp()->getTwitchPubSub()->raid.updated,
        [](const QJsonObject &payload) {
            if (!getSettings()->satorinoRaidsOpenTab.getValue() ||
                isAppAboutToQuit())
                return;
            const auto receivedAt = QDateTime::currentMSecsSinceEpoch();
            postToThread([payload, receivedAt] {
                if (auto *app = tryGetApp(); app && !isAppAboutToQuit())
                    app->getWindows()->handleSatorinoRaid(payload, receivedAt);
            });
        });
}
RaidStatusContext RaidController::statusContext() const
{
    assertInGuiThread();
    RaidStatusContext status;
    status.enabled = getSettings()->satorinoRaidsOpenTab.getValue();
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    status.loggedIn =
        account && !account->isAnon() && !account->getOAuthToken().isEmpty();
    status.lastEvent = this->lastEvent_;
    if (!status.enabled)
        return status;
    std::set<QString> seen;
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  const auto id = channel->roomId();
                  const auto key =
                      id.isEmpty() ? "login:" + channel->getName() : "id:" + id;
                  if (!seen.insert(key).second)
                      return;
                  if (validRaidId(id) &&
                      getApp()->getTwitchPubSub()->isListeningToRaids(id))
                      ++status.listening;
                  else
                      ++status.waitingForIds;
              });
    return status;
}
QString RaidController::record(const QString &source,
                               const std::optional<RaidEvent> &event,
                               const QString &result, bool synthetic)
{
    QString name = source;
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  if (sourceId(channel, account) == source)
                      name = channel->getName();
              });
    const auto target =
        !event ? QString("unknown target")
        : event->targetName.isEmpty() || event->targetName == event->targetLogin
            ? event->targetLogin
            : event->targetName + " (" + event->targetLogin + ")";
    const auto text =
        QString("Last raid: %1 -> %2 at %3, %4%5")
            .arg(name, target, QDateTime::currentDateTime().toString("HH:mm"),
                 synthetic ? "test: " : "", result);
    this->lastEvent_ = text;
    if (text != this->lastStatus_)
    {
        this->lastStatus_ = text;
        qCInfo(chatterinoPubSub).noquote() << text;
    }
    return text;
}
QString RaidController::receive(const QJsonObject &payload, qint64 receivedAtMs)
{
    assertInGuiThread();
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
        return "Raid watch: off";
    QString error;
    const auto parsed = parseRaidFeed(payload, receivedAtMs, &error);
    if (!parsed)
        return this->record({}, {}, "ignored (" + error + ")");
    if (receivedAtMs < this->enabledAtUtcMs_)
        return this->record(parsed->sourceId, parsed->event,
                            "ignored (received before watch started)");
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon() || account->getUserId() != this->ownerId_)
        return this->record(parsed->sourceId, parsed->event,
                            "ignored (account changed or not logged in)");
    std::set<QString> open;
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  const auto id = sourceId(channel, account);
                  if (validRaidId(id))
                      open.insert(id);
              });
    this->feed_.retain(open);
    if (!open.contains(parsed->sourceId))
        return this->record(parsed->sourceId, parsed->event,
                            "ignored (source is no longer open)");
    const auto outcome = this->feed_.observe(payload, receivedAtMs);
    this->scheduleDeadline();
    if (outcome.action == RaidFeedAction::Fire && outcome.event)
        return this->act(*outcome.event, receivedAtMs, outcome.reason, false);
    return this->record(outcome.sourceId, outcome.event,
                        outcome.action == RaidFeedAction::Cancelled
                            ? "cancelled"
                        : outcome.action == RaidFeedAction::Wait
                            ? "countdown; waiting for go"
                            : "ignored (" + outcome.reason + ")");
}
void RaidController::scheduleDeadline()
{
    this->deadlineTimer_.stop();
    const auto next = this->feed_.nextDeadline();
    if (!next || !getSettings()->satorinoRaidsOpenTab.getValue())
        return;
    // One-second grace lets a queued cancel/go at the countdown boundary arrive first.
    const auto wait =
        std::clamp(*next + 1000 - QDateTime::currentMSecsSinceEpoch(),
                   qint64(1), qint64(std::numeric_limits<int>::max()));
    this->deadlineTimer_.start(static_cast<int>(wait));
}
void RaidController::deadlineExpired()
{
    if (!getSettings()->satorinoRaidsOpenTab.getValue() || isAppAboutToQuit())
        return;
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    std::set<QString> open;
    visitOpen(this->windows_,
              [&](Window *, SplitContainer *, Split *, const auto &channel) {
                  const auto id = sourceId(channel, account);
                  if (validRaidId(id))
                      open.insert(id);
              });
    this->feed_.retain(open);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &outcome : this->feed_.expire(now))
        if (outcome.event)
            this->act(*outcome.event, now, outcome.reason, false);
    this->scheduleDeadline();
}
QString RaidController::act(const RaidEvent &inputEvent, qint64 receivedAtMs,
                            const QString &trigger, bool synthetic)
{
    assertInGuiThread();
    if (isAppAboutToQuit())
        return "Raid watch: shutting down";
    const auto *event = &inputEvent;
    const auto ownerId = this->ownerId_;
    const auto finish = [&](const QString &result) {
        return this->record(event->sourceId, inputEvent,
                            result + " (" + trigger + ")", synthetic);
    };
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!getSettings()->satorinoRaidsOpenTab.getValue())
        return finish("ignored (setting off)");
    if (!account || account->isAnon() || account->getOAuthToken().isEmpty())
        return finish("ignored (not logged in)");
    if (account->getUserId() != ownerId)
        return finish("ignored (account changed)");
    QPointer<Window> sourceWindow;
    QPointer<Window> targetWindow;
    QPointer<SplitContainer> targetPage;
    QPointer<Split> targetSplit;
    visitOpen(this->windows_, [&](Window *window, SplitContainer *page,
                                  Split *split, const auto &channel) {
        if (sourceId(channel, account) == event->sourceId && !sourceWindow)
        {
            sourceWindow = window;
        }
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
QString RaidController::simulateKnown(const ChannelPtr &source,
                                      const QString &login, const QString &id)
{
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    const auto channel = std::dynamic_pointer_cast<TwitchChannel>(source);
    if (!channel || source->getType() != Channel::Type::Twitch)
        return "Raid test requires the current real Twitch channel";
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const QJsonObject payload{
        {"type", "raid_go_v2"},
        {"topic", "raid." + sourceId(channel, account)},
        {"raid", QJsonObject{{"id", "test-" + QString::number(now)},
                             {"source_id", sourceId(channel, account)},
                             {"target_id", id},
                             {"target_login", login},
                             {"target_display_name", login},
                             {"viewer_count", 0}}}};
    QString error;
    const auto event = parseRaidEvent(payload, now, &error);
    return event ? this->act(*event, now, "go", true)
                 : "Raid test ignored: " + error;
}
QString RaidController::simulate(const ChannelPtr &source,
                                 const QString &targetLogin,
                                 const QString &targetId)
{
    assertInGuiThread();
    if (!getSettings()->satorinoRaidsOpenTab.getValue())
        return "Raid test ignored: setting off";
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (!account || account->isAnon() || account->getUserId() != this->ownerId_)
        return "Raid test ignored: not logged in or account changed";
    if (!source || source->getType() != Channel::Type::Twitch)
        return "Raid test requires the current real Twitch channel";
    QString error;
    const auto command = parseRaidTestCommand(
        QStringList{"/satorino-testraid", targetLogin} +
            (targetId.isEmpty() ? QStringList{} : QStringList{targetId}),
        &error);
    if (!command)
        return error;
    const auto known = std::dynamic_pointer_cast<TwitchChannel>(
        getApp()->getTwitch()->getChannelOrEmpty(command->login));
    const auto id =
        raidTestKnownId(*command, known ? known->roomId() : QString{});
    if (!id.isEmpty())
        return this->simulateKnown(source, command->login, id);
    const QPointer<RaidController> guard(this);
    const std::weak_ptr<Channel> weak = source;
    const auto generation = this->generation_;
    const auto owner = this->ownerId_;
    const auto done =
        std::make_shared<bool>(false);  // Accessed only on the GUI thread.
    const auto finish = [guard, weak, generation, owner, done,
                         login = command->login](RaidLookupStatus status,
                                                 const QString &returnedId,
                                                 const QString &returnedLogin) {
        if (!guard || *done || isAppAboutToQuit())
            return;
        *done = true;
        const auto channel = weak.lock();
        if (!channel)
            return;
        const auto current = getApp()->getAccounts()->twitch.getCurrent();
        if (!getSettings()->satorinoRaidsOpenTab.getValue() ||
            guard->generation_ != generation || !current || current->isAnon() ||
            current->getUserId() != owner)
        {
            channel->addSystemMessage(
                "Raid test ignored: setting/account changed during lookup");
            return;
        }
        const auto result =
            raidLookupResult(login, status, returnedId, returnedLogin);
        channel->addSystemMessage(
            result.usable ? guard->simulateKnown(channel, login, result.id)
                          : "Raid test: " + result.reason);
    };
    QTimer::singleShot(10000, this, [finish] {
        finish(RaidLookupStatus::Timeout, {}, {});
    });
    // Exactly one normal users request for this explicit command, never for passive feed events.
    getHelix()->fetchUsers(
        {}, QStringList{command->login},
        [finish](const std::vector<HelixUser> &users) {
            const auto status = users.empty() ? RaidLookupStatus::NotFound
                                              : RaidLookupStatus::Found;
            const auto id = users.empty() ? QString{} : users.front().id;
            const auto login = users.empty() ? QString{} : users.front().login;
            postToThread([finish, status, id, login] {
                finish(status, id, login);
            });
        },
        [finish] {
            postToThread([finish] {
                finish(RaidLookupStatus::Error, {}, {});
            });
        });
    return "Raid test: looking up " + command->login +
           " once; no raid or chat message is sent";
}
}  // namespace chatterino::satorino
