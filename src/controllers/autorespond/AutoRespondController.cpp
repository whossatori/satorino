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

#include <algorithm>
#include <cmath>

namespace chatterino::satorino {
namespace {

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
    auto *accounts = getApp()->getAccounts();
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
    if (!channel.canSendMessage() || currentIdentity(channel).id.isEmpty())
        return false;
    if (channel.getType() == Channel::Type::Twitch)
    {
        const auto &twitch = static_cast<const TwitchChannel &>(channel);
        return !twitch.isAnonymous() && twitch.isAutoRespondConnected() &&
               getApp()->getTwitch()
                       ->getChannelOrEmpty(channel.getName())
                       .get() == &channel;
    }
    if (channel.getType() == Channel::Type::Kick)
    {
        const auto &kick = static_cast<const KickChannel &>(channel);
        return kick.roomID() != 0 &&
               getApp()->getKickChatServer()->findBySlug(kick.slug()).get() ==
                   &channel &&
               getApp()->getKickChatServer()->liveUpdates().isRoomConnected(
                   kick.roomID());
    }
    return false;
}

std::vector<ChannelPtr> timerTargets(const QString &key)
{
    std::vector<ChannelPtr> channels;
    getApp()->getTwitch()->forEachChannel([&](const ChannelPtr &channel) {
        if (channel->getType() == Channel::Type::Twitch &&
            (key == "*" || channelKey(*channel) == key) && ready(*channel))
            channels.push_back(channel);
    });
    getApp()->getKickChatServer()->forEachChannel([&](KickChannel &channel) {
        if ((key == "*" || channelKey(channel) == key) && ready(channel))
            channels.push_back(channel.sharedFromThis());
    });
    // Stable ordering makes a global cap predictable when it truncates a broadcast.
    std::ranges::sort(channels, {}, [](const auto &channel) {
        return channelKey(*channel);
    });
    return channels;
}

}  // namespace

AutoRespondController::AutoRespondController(QObject *parent)
    : QObject(parent)
{
    this->clock_.start();
    this->timer_.setInterval(1000);
    this->timer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->timer_, &QTimer::timeout, this,
                     &AutoRespondController::tick);
    auto &settings = autoRespondSettings();
    settings.enabled.connect(
        [this] {
            this->resetSchedule();
        },
        this->connections_);
    settings.triggers.connect(
        [this] {
            this->rebuildTriggers();
        },
        this->connections_);
    settings.timers.connect(
        [this] {
            this->resetSchedule();
        },
        this->connections_, false);
    auto *accounts = getApp()->getAccounts();
    this->twitchConnection_ =
        accounts->twitch.currentUserChanged.connect([this] {
            this->resetSchedule();
        });
    this->connections_.managedConnect(accounts->kick.currentUserChanged,
                                      [this] {
                                          this->resetSchedule();
                                      });
}

void AutoRespondController::rebuildTriggers()
{
    assertInGuiThread();
    ++this->revision_;
    this->triggers_.clear();
    for (const auto &rule : autoRespondSettings().triggers.getValueCopy())
    {
        if (!rule.enabled || rule.id.isEmpty() || rule.pattern.isEmpty())
            continue;
        const auto expression = autoRespondExpression(rule);
        if (expression.isValid())
            this->triggers_.emplace_back(rule, expression);
    }
}

void AutoRespondController::resetSchedule()
{
    assertInGuiThread();
    ++this->revision_;
    this->previous_ = QDateTime::currentDateTime();
    this->previousSteady_ = this->clock_.elapsed();
    if (autoRespondSettings().enabled.getValue())
        this->timer_.start();
    else
        this->timer_.stop();
}

void AutoRespondController::receive(const ChannelPtr &channel,
                                    const MessagePtr &message)
{
    assertInGuiThread();
    auto &settings = autoRespondSettings();
    if (!settings.enabled.getValue() || !ready(*channel))
        return;
    // Also reject stale unflagged replay traffic, without inspecting old chat.
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
    // EventSub/IRC duplicates cannot schedule two responses to the same ID.
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
                               // No send after off/on, rule edits, an account switch, suspend,
                               // teardown, or a closed/disconnected channel.
                               if (revision != this->revision_ ||
                                   this->clock_.elapsed() - due > 2500)
                                   return;
                               if (const auto target = weak.lock())
                                   this->submit(target, text, account,
                                                rule.sound, rule.customSound);
                           });
    }
}

void AutoRespondController::submit(const ChannelPtr &channel,
                                   const QString &text, const QString &identity,
                                   bool sound, const QString &customSound)
{
    assertInGuiThread();
    auto &settings = autoRespondSettings();
    if (!settings.enabled.getValue() || !ready(*channel) ||
        currentIdentity(*channel).id != identity)
        return;
    const auto clean = sanitizeAutoRespond(text);
    if (clean.isEmpty() || !this->limits_.takeGlobal(
                               std::clamp(settings.perMinute.getValue(), 1, 60),
                               this->clock_.elapsed()))
        return;
    this->limits_.rememberSent(channelKey(*channel), clean,
                               this->clock_.elapsed());
    const auto sent = sendAutoRespond(*channel, clean);
    // Remember the actual expanded/duplicate-adjusted wire text as well.
    if (!sent.prepared.isEmpty())
        this->limits_.rememberSent(channelKey(*channel), sent.prepared,
                                   this->clock_.elapsed());
    if (sent.prepared.isEmpty() || sent.accepted == false)
        return;
    // Channel::sendMessage is void and remote acceptance is asynchronous. This
    // reports submission truthfully; normal limiter/API errors remain visible.
    if (settings.showNote.getValue())
        channel->addSystemMessage("auto respond submitted: " + clean);
    if (sound)
        getApp()->getSound()->play(customSound.isEmpty()
                                       ? QUrl("qrc:/sounds/ping2.wav")
                                       : QUrl(customSound));
}

void AutoRespondController::tick()
{
    assertInGuiThread();
    auto &settings = autoRespondSettings();
    if (!settings.enabled.getValue())
        return;
    const auto now = QDateTime::currentDateTime();
    const auto steady = this->clock_.elapsed();
    const auto zone = QTimeZone::systemTimeZone();
    auto dates = settings.consumedDates.getValueCopy();
    for (const auto &rule : settings.timers.getValueCopy())
    {
        if (rule.id.isEmpty() || !rule.enabled)
            continue;
        auto &consumed = this->consumed_[rule.id];
        if (const auto found = dates.find(rule.id.toStdString());
            found != dates.end())
        {
            const auto saved = QDate::fromString(found->second, Qt::ISODate);
            if (saved.isValid() && (!consumed.isValid() || saved > consumed))
                consumed = saved;
        }
        const auto next =
            nextAutoRespondFire(rule, this->previous_, consumed, zone);
        if (!next || *next > now)
            continue;
        const bool due =
            autoRespondTimerDue(rule, this->previous_, now, consumed, zone) &&
            steady - this->previousSteady_ <= 2500 &&
            std::abs(this->previous_.msecsTo(now) -
                     (steady - this->previousSteady_)) < 1000;
        // Consume even when disconnected, logged out, capped or missed. The
        // persisted date also prevents a restart/clock rollback duplicating a day.
        consumed = next->toTimeZone(zone).date();
        dates[rule.id.toStdString()] = consumed.toString(Qt::ISODate);
        settings.consumedDates.setValue(dates);
        if (!due)
            continue;
        for (const auto &channel : timerTargets(rule.channel))
        {
            const auto text = rule.message;
            // Timers have no sender/match; {channel} is still useful for broadcasts.
            AutoRespondInput input;
            input.channel = channelKey(*channel);
            this->submit(
                channel,
                expandAutoRespond(text, input, QRegularExpressionMatch()),
                currentIdentity(*channel).id);
        }
    }
    this->previous_ = now;
    this->previousSteady_ = steady;
}

AutoRespondController &autoRespond()
{
    assertInGuiThread();
    static QPointer<AutoRespondController> controller;
    if (!controller)
        controller = new AutoRespondController(QCoreApplication::instance());
    return *controller;
}

void observeAutoRespond(Channel &channel, const MessagePtr &message,
                        MessageContext context,
                        std::optional<MessageFlags> overridingFlags)
{
    if ((channel.getType() != Channel::Type::Twitch &&
         channel.getType() != Channel::Type::Kick) ||
        !isAutoRespondLiveMessage(context, message->flags) ||
        (overridingFlags &&
         !isAutoRespondLiveMessage(context, *overridingFlags)) ||
        getApp()->isTest())
        return;
    // Make even an off-thread producer safe; no matcher/settings/UI work there.
    runInGuiThread([weak = channel.weak_from_this(), message] {
        if (auto target = weak.lock())
        {
            if (autoRespondSettings().enabled.getValue())
                autoRespond().receive(target, message);
        }
    });
}

}  // namespace chatterino::satorino
