// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondController.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/enums/MessageContext.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/autorespond/AutoRespondSend.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/kick/KickAccount.hpp"
#include "providers/kick/KickChatServer.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "util/PostToThread.hpp"

#include <QCoreApplication>
#include <QPointer>
#include <QScopeGuard>

#include <algorithm>
#include <cmath>
#include <atomic>

namespace chatterino::satorino {
namespace {
std::atomic_bool observeEnabled{false};
std::atomic_uint64_t observationGeneration{0};
AutoRespondController *activeController = nullptr;

QString channelKey(const Channel &channel)
{
    switch (channel.getType())
    {
        case Channel::Type::Twitch:
            return "twitch:" + channel.getName().toLower();
        case Channel::Type::Kick:
            return "kick:" + channel.getName().toLower();
        default:
            return {};
    }
}

struct Identity {
    QString id;
    QString name;
};
Identity currentIdentity(const Channel &channel)
{
    auto *app = tryGetApp();
    auto *accounts = app ? app->getAccounts() : nullptr;
    if (!accounts)
        return {};
    if (channel.getType() == Channel::Type::Twitch &&
        accounts->twitch.isLoggedIn())
    {
        const auto account = accounts->twitch.getCurrent();
        if (account && !account->isAnon())
            return {account->getUserId(), account->getUserName()};
    }
    if (channel.getType() == Channel::Type::Kick && accounts->kick.isLoggedIn())
    {
        const auto account = accounts->kick.current();
        if (account && !account->isAnonymous())
            return {QString::number(account->userID()), account->username()};
    }
    return {};
}

bool ready(const Channel &channel)
{
    auto *app = tryGetApp();
    if (!app || isAppAboutToQuit() || !app->getAccounts())
        return false;
    if (!channel.canSendMessage() || currentIdentity(channel).id.isEmpty())
        return false;
    if (channel.getType() == Channel::Type::Twitch)
    {
        const auto *typed = dynamic_cast<const TwitchChannel *>(&channel);
        auto *app = tryGetApp();
        auto *server = app ? app->getTwitch() : nullptr;
        if (!typed || !server)
            return false;
        const auto &twitch = *typed;
        return !twitch.isAnonymous() && twitch.isAutoRespondConnected() &&
               server->getChannelOrEmpty(channel.getName()).get() == &channel;
    }
    if (channel.getType() == Channel::Type::Kick)
    {
        const auto *typed = dynamic_cast<const KickChannel *>(&channel);
        auto *app = tryGetApp();
        auto *server = app ? app->getKickChatServer() : nullptr;
        if (!typed || !server)
            return false;
        const auto &kick = *typed;
        return kick.roomID() != 0 &&
               server->findBySlug(kick.slug()).get() == &channel &&
               server->liveUpdates().isRoomConnected(kick.roomID());
    }
    return false;
}

std::vector<ChannelPtr> timerTargets(const QString &key)
{
    std::vector<ChannelPtr> candidates;
    auto *app = tryGetApp();
    if (!app)
        return candidates;
    // Enumeration holds Twitch's channelMutex. Filter after taking a snapshot;
    // ready()/getChannelOrEmpty would recursively acquire that same mutex.
    if (auto *server = app->getTwitch())
        server->forEachChannel([&](ChannelPtr channel) {
            if (channel)
                candidates.push_back(std::move(channel));
        });
    if (auto *server = app->getKickChatServer())
        server->forEachChannel([&](KickChannel &channel) {
            if (auto ptr = channel.weakFromThis().lock())
                candidates.push_back(std::move(ptr));
        });
    std::erase_if(candidates, [&](const auto &channel) {
        return (key != "*" && channelKey(*channel) != key) || !ready(*channel);
    });
    std::ranges::sort(candidates, {}, [](const auto &channel) {
        return channelKey(*channel);
    });
    return candidates;
}

}  // namespace

AutoRespondController::AutoRespondController(IApplication &app)
    : app_(app)
{
    this->clock_.start();
    this->timer_.setInterval(1000);
    this->timer_.setTimerType(Qt::PreciseTimer);
    this->queueTimer_.setSingleShot(true);
    this->queueTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->timer_, &QTimer::timeout, this,
                     &AutoRespondController::tick);
    QObject::connect(&this->queueTimer_, &QTimer::timeout, this,
                     &AutoRespondController::processQueue);
    this->settings_.enabled.connect(
        [this] {
            this->resetSchedule();
        },
        this->connections_);
    this->settings_.rules.connect(
        [this] {
            this->rebuildTriggers();
            this->resetSchedule(false);
        },
        this->connections_);
    if (auto *accounts = this->app_.getAccounts())
    {
        this->twitchConnection_ =
            accounts->twitch.currentUserChanged.connect([this] {
                this->resetSchedule();
            });
        this->connections_.managedConnect(accounts->kick.currentUserChanged,
                                          [this] {
                                              this->resetSchedule();
                                          });
    }
}
AutoRespondController::~AutoRespondController()
{
    this->shutdown();
}
void AutoRespondController::start()
{
    assertInGuiThread();
    if (this->running_ || this->app_.isTest() || isAppAboutToQuit())
        return;
    activeController = this;
    this->running_ = true;
    this->resetSchedule();
}
void AutoRespondController::shutdown()
{
    assertInGuiThread();
    observeEnabled.store(false);
    this->running_ = false;
    ++this->revision_;
    observationGeneration.fetch_add(1);
    this->timer_.stop();
    this->queueTimer_.stop();
    this->clearQueue();
    this->connections_.clear();
    this->twitchConnection_.disconnect();
    if (activeController == this)
        activeController = nullptr;
}
void AutoRespondController::resetSchedule(bool invalidate)
{
    assertInGuiThread();
    observeEnabled.store(this->running_ && this->settings_.enabled.getValue());
    if (invalidate)
    {
        ++this->revision_;
        observationGeneration.fetch_add(1);
        this->queueTimer_.stop();
        this->clearQueue();
    }
    this->previous_ = QDateTime::currentDateTime();
    this->previousSteady_ = this->clock_.elapsed();
    const bool enabled = this->running_ && this->settings_.enabled.getValue();
    observeEnabled.store(enabled);
    if (enabled)
        this->timer_.start();
    else
        this->timer_.stop();
}
void AutoRespondController::rebuildTriggers()
{
    assertInGuiThread();
    this->triggers_.clear();
    for (const auto &rule : this->settings_.rules.getValueCopy())
    {
        if (rule.type != AutoRespondType::Trigger || !rule.enabled ||
            rule.id.isEmpty() || rule.pattern.isEmpty())
            continue;
        const auto expression = autoRespondExpression(rule);
        if (expression.isValid())
            this->triggers_.emplace_back(rule, expression);
    }
}
bool AutoRespondController::ruleCurrent(const AutoRespondRule &rule) const
{
    const auto rules = this->settings_.rules.getValueCopy();
    return std::ranges::any_of(rules, [&](const auto &current) {
        return current.id == rule.id && current == rule && current.enabled;
    });
}
void AutoRespondController::receive(const ChannelPtr &channel,
                                    const MessagePtr &message)
{
    assertInGuiThread();
    if (!this->running_ || !this->settings_.enabled.getValue() || !channel ||
        !message || !ready(*channel))
        return;
    if (message->serverReceivedTime.isValid() &&
        message->serverReceivedTime.secsTo(QDateTime::currentDateTimeUtc()) >
            60)
        return;
    const auto now = this->clock_.elapsed();
    const auto identity = currentIdentity(*channel);
    const auto key = channelKey(*channel);
    AutoRespondInput input{
        key,
        message->loginName,
        message->messageText,
        true,
        true,
        false,
        message->userID == identity.id ||
            message->loginName.compare(identity.name, Qt::CaseInsensitive) == 0,
        this->limits_.wasAutoSent(key, message->messageText, now)};
    if (!canAutoRespond(input))
        return;
    while (!this->seen_.empty() && now - this->seen_.front().at >= 300000)
        this->seen_.pop_front();
    if (!message->id.isEmpty())
    {
        const auto seenKey = key + u'\n' + message->id;
        if (std::ranges::any_of(this->seen_, [&](const auto &seen) {
                return seen.key == seenKey;
            }))
            return;
        this->seen_.push_back({seenKey, now});
        while (this->seen_.size() > 4096)
            this->seen_.pop_front();
    }
    for (const auto &[rule, expression] : this->triggers_)
    {
        const auto match = matchAutoRespond(rule, input, expression);
        if (!match)
            continue;
        const auto text = expandAutoRespond(rule.response, input, *match);
        if (text.isEmpty() ||
            !this->limits_.reserveRule(rule.id, rule.cooldownSeconds, now))
            continue;
        const auto revision = this->revision_;
        const auto due = now + std::clamp(rule.delayMs, 0, 3600000);
        QTimer::singleShot(std::clamp(rule.delayMs, 0, 3600000), this,
                           [this, weak = std::weak_ptr<Channel>(channel),
                            revision, due, rule, text, account = identity.id] {
                               if (!this->running_ ||
                                   revision != this->revision_ ||
                                   !this->settings_.enabled.getValue() ||
                                   this->clock_.elapsed() - due > 2500 ||
                                   !this->ruleCurrent(rule))
                                   return;
                               if (auto target = weak.lock())
                                   this->enqueue(target, rule, text, account);
                           });
    }
}
void AutoRespondController::enqueue(
    const ChannelPtr &channel, const AutoRespondRule &rule, const QString &text,
    const QString &account, const std::shared_ptr<AutoRespondBatch> &batch)
{
    this->queue_.push({++this->nextToken_,
                       this->revision_,
                       channel,
                       rule,
                       sanitizeAutoRespond(text),
                       account,
                       batch,
                       {}});
    if (!this->queueTimer_.isActive())
        this->queueTimer_.start(0);
}
void AutoRespondController::clearQueue()
{
    const auto pending = this->queue_.clear();
    for (const auto &item : pending)
        if (item.token != this->activeToken_)
            this->finishBatch(item, false);
}
void AutoRespondController::finishBatch(const AutoRespondQueuedSend &item,
                                        bool sent)
{
    if (!item.batch)
    {
        if (!sent && this->running_ && this->settings_.showNote.getValue())
            this->postNote(item.channel,
                           "auto respond sent 0 of 1 (1 skipped)");
        return;
    }
    auto &batch = *item.batch;
    if (sent)
        ++batch.sent;
    else
        ++batch.skipped;
    if (--batch.remaining != 0 || !this->running_ ||
        !this->settings_.showNote.getValue())
        return;
    for (const auto &weak : batch.targets)
        if (auto target = weak.lock())
        {
            this->postNote(target,
                           QString("auto respond sent %1 of %2 (%3 skipped)")
                               .arg(batch.sent)
                               .arg(batch.total)
                               .arg(batch.skipped));
            break;
        }
}
void AutoRespondController::postNote(std::weak_ptr<Channel> channel,
                                     QString text)
{
    QTimer::singleShot(
        0, this, [this, channel = std::move(channel), text = std::move(text)] {
            if (!this->running_ || isAppAboutToQuit())
                return;
            if (auto target = channel.lock())
                target->addSystemMessage(text);
        });
}
void AutoRespondController::processQueue()
{
    assertInGuiThread();
    if (this->processing_ || !this->running_ ||
        !this->settings_.enabled.getValue() || isAppAboutToQuit())
        return;
    this->processing_ = true;
    const QPointer<AutoRespondController> self(this);
    const auto guard = qScopeGuard([self] {
        if (self)
            self->processing_ = false;
    });
    while (!this->queue_.empty())
    {
        const auto now = this->clock_.elapsed();
        this->queue_.startHead(now);
        const auto item = this->queue_.front();
        const auto channel = item.channel.lock();
        if (item.session != this->revision_ || !channel || !ready(*channel) ||
            currentIdentity(*channel).id != item.account ||
            !this->ruleCurrent(item.rule) || item.text.isEmpty() ||
            this->queue_.expired(now))
        {
            this->queue_.finish(item.token, now, false);
            this->finishBatch(item, false);
            continue;
        }
        qint64 providerWait = 0;
        bool ordinaryTwitch = false;
        if (auto twitch = std::dynamic_pointer_cast<TwitchChannel>(channel))
        {
            auto *server =
                dynamic_cast<TwitchIrcServer *>(this->app_.getTwitch());
            if (!server)
            {
                this->queue_.finish(item.token, now, false);
                this->finishBatch(item, false);
                continue;
            }
            providerWait = server->autoRespondSendWait(twitch).count();
            ordinaryTwitch = !twitch->hasHighRateLimit();
        }
        else if (auto kick = std::dynamic_pointer_cast<KickChannel>(channel))
            providerWait = kick->autoRespondSendWait().count();
        const auto wait = this->queue_.wait(now, ordinaryTwitch, providerWait);
        if (wait > 0)
        {
            const auto remaining =
                std::max(qint64(1), 120000 - (now - *item.startedAt));
            this->queueTimer_.start(
                static_cast<int>(std::min(wait, remaining)));
            return;
        }
        this->activeToken_ = item.token;
        const auto result = sendAutoRespond(*channel, item.text);
        if (!self)
            return;  // a re-entrant upstream observer may have destroyed the app
        this->activeToken_ = 0;
        const bool valid = this->running_ &&
                           this->settings_.enabled.getValue() &&
                           item.session == this->revision_;
        if (result.accepted == false && valid)
        {
            this->queue_.refused(this->clock_.elapsed());
            const auto remaining = std::max(
                qint64(1), 120000 - (this->clock_.elapsed() - *item.startedAt));
            this->queueTimer_.start(
                static_cast<int>(std::min(qint64(1100), remaining)));
            return;
        }
        const bool sent = result.accepted == true && !result.prepared.isEmpty();
        this->queue_.finish(item.token, this->clock_.elapsed() + 1, sent);
        this->finishBatch(item, sent);
        if (sent)
        {
            this->limits_.rememberSent(channelKey(*channel), item.text,
                                       this->clock_.elapsed());
            this->limits_.rememberSent(channelKey(*channel), result.prepared,
                                       this->clock_.elapsed());
            if (!item.batch && this->settings_.showNote.getValue())
                this->postNote(channel, "auto respond submitted: " + item.text);
            if (item.rule.sound)
                QTimer::singleShot(
                    0, this, [this, sound = item.rule.customSound] {
                        if (this->running_)
                            if (auto *player = this->app_.getSound())
                                player->play(sound.isEmpty()
                                                 ? QUrl("qrc:/sounds/ping2.wav")
                                                 : QUrl(sound));
                    });
        }
        if (!valid)
            return;
        if (sent)
        {
            if (!this->queue_.empty())
                this->queueTimer_.start(0);
            return;
        }
    }
}
void AutoRespondController::tick()
{
    assertInGuiThread();
    if (!this->running_ || !this->settings_.enabled.getValue() ||
        !this->previous_.isValid())
        return;
    const QPointer<AutoRespondController> self(this);
    const auto now = QDateTime::currentDateTime();
    const auto steady = this->clock_.elapsed();
    const auto zone = QTimeZone::systemTimeZone();
    auto dates = this->settings_.consumedDates.getValueCopy();
    for (const auto &rule : this->settings_.rules.getValueCopy())
    {
        if (rule.type != AutoRespondType::Timer || !rule.enabled ||
            rule.id.isEmpty())
            continue;
        auto &consumed = this->consumed_[rule.id];
        if (const auto found = dates.find(rule.id.toStdString());
            found != dates.end())
        {
            const auto saved = QDate::fromString(found->second, Qt::ISODate);
            if (saved.isValid() && (!consumed.isValid() || saved > consumed))
                consumed = saved;
        }
        const auto timerRule = rule.timerRule();
        const auto next =
            nextAutoRespondFire(timerRule, this->previous_, consumed, zone);
        if (!next || *next > now)
            continue;
        const bool due = autoRespondTimerDue(timerRule, this->previous_, now,
                                             consumed, zone) &&
                         steady - this->previousSteady_ <= 2500 &&
                         std::abs(this->previous_.msecsTo(now) -
                                  (steady - this->previousSteady_)) < 1000;
        consumed = next->toTimeZone(zone).date();
        dates[rule.id.toStdString()] = consumed.toString(Qt::ISODate);
        this->settings_.consumedDates.setValue(
            dates);  // one occurrence, before queuing the broadcast
        if (!self || !this->running_)
            return;
        if (!due)
            continue;
        const auto targets = timerTargets(rule.channel);
        std::shared_ptr<AutoRespondBatch> batch;
        if (rule.channel == "*" && !targets.empty())
        {
            batch = std::make_shared<AutoRespondBatch>();
            batch->total = static_cast<int>(targets.size());
            batch->remaining = batch->total;
            for (const auto &target : targets)
                batch->targets.push_back(target);
        }
        for (const auto &target : targets)
        {
            AutoRespondInput input;
            input.channel = channelKey(*target);
            this->enqueue(target, rule,
                          expandAutoRespond(rule.response, input,
                                            QRegularExpressionMatch()),
                          currentIdentity(*target).id, batch);
        }
    }
    this->previous_ = now;
    this->previousSteady_ = steady;
}
AutoRespondController *tryAutoRespond()
{
    return activeController;
}
AutoRespondSettings *tryAutoRespondSettings()
{
    return activeController ? &activeController->settings() : nullptr;
}
void observeAutoRespond(Channel &channel, const MessagePtr &message,
                        MessageContext context,
                        std::optional<MessageFlags> overridingFlags)
{
    if (!observeEnabled.load() || isAutoRespondSend())
        return;
    if (!message ||
        (channel.getType() != Channel::Type::Twitch &&
         channel.getType() != Channel::Type::Kick) ||
        !isAutoRespondLiveMessage(context, message->flags) ||
        (overridingFlags &&
         !isAutoRespondLiveMessage(context, *overridingFlags)))
        return;
    auto *contextObject = QCoreApplication::instance();
    if (!contextObject || QCoreApplication::closingDown())
        return;
    const auto generation = observationGeneration.load();
    QMetaObject::invokeMethod(
        contextObject,
        [weak = channel.weak_from_this(), message, generation] {
            if (!observeEnabled.load() ||
                generation != observationGeneration.load() ||
                isAutoRespondSend())
                return;
            if (auto *controller = tryAutoRespond())
                if (auto target = weak.lock())
                    controller->receive(target, message);
        },
        Qt::QueuedConnection);
}
}  // namespace chatterino::satorino
