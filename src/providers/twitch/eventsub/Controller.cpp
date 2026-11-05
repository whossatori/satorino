// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/eventsub/Controller.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/QLogging.hpp"
#include "common/network/NetworkCommon.hpp"   // satorino: DELETE request type
#include "common/network/NetworkRequest.hpp"  // satorino: late raid POST cleanup
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchAccountManager.hpp"
#include "common/Version.hpp"
#include "debug/AssertInGuiThread.hpp"  // satorino: capture account data before posting
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/eventsub/Connection.hpp"
#include "util/QMagicEnum.hpp"
#include "util/RenameThread.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/certify/https_verification.hpp>
#include <twitch-eventsub-ws/session.hpp>
#include <QUrl>  // satorino: late cleanup endpoint
#include <QUrlQuery>

#include <memory>
#include <mutex>
#include <utility>

namespace {

using namespace chatterino;
using namespace chatterino::eventsub;

std::tuple<std::string, std::string, std::string> getEventSubHost()
{
#ifndef NDEBUG
    if (getApp()->getArgs().useLocalEventsub)
    {
        return {"localhost", "3012", "/ws"};
    }
#endif

    return {"eventsub.wss.twitch.tv", "443", "/ws"};
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
const auto &LOG = chatterinoTwitchEventSub;

// satorino: a late success must be deleted with the POST's original account,
// even if normal Helix auth has already changed. No token appears in logging.
void deleteLateRaid(const QString &id, const QString &client,
                    const QString &token)
{
    QUrl url("https://api.twitch.tv/helix/eventsub/subscriptions");
    QUrlQuery query;
    query.addQueryItem("id", id);
    url.setQuery(query);
    NetworkRequest(url, NetworkRequestType::Delete)
        .header("Client-ID", client)
        .header("Authorization", QString("Bearer ") + token)
        .sensitive()
        .followRedirects(false)
        .timeout(15000)
        .onError([](const NetworkResult &) {
            qCWarning(LOG) << "Late raid subscription cleanup failed";
        })
        .execute();
}
}  // namespace

namespace chatterino::eventsub {

using namespace std::literals::chrono_literals;

class QLogProxy : public lib::Logger
{
public:
    QLogProxy(const QLoggingCategory &loggingCategory_)
        : loggingCategory(loggingCategory_)
    {
    }

    void debug(std::string_view msg) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        qCDebug(this->loggingCategory).noquote() << msg;
#endif
    }

    void warn(std::string_view msg) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        qCWarning(this->loggingCategory).noquote() << msg;
#endif
    }

private:
    const QLoggingCategory &loggingCategory;
};

Controller::Controller()
    : logProxy(std::shared_ptr<lib::Logger>(new QLogProxy(LOG())))
    , userAgent(QStringLiteral("satorino/%1 (%2)")
                    .arg(Version::instance().version(),
                         Version::instance().commitHash())
                    .toUtf8()
                    .toStdString())
    , ioContext(1)
    , work(boost::asio::make_work_guard(this->ioContext))
{
    std::tie(this->eventSubHost, this->eventSubPort, this->eventSubPath) =
        getEventSubHost();
    this->thread = std::make_unique<std::thread>([this] {
        // make sure we set in any case, even exceptions
        auto guard = qScopeGuard([&] {
            this->stoppedFlag.set();
        });

        this->ioContext.run();
    });
    renameThread(*this->thread, "C2EventSub");

    this->threadGuard = std::make_unique<ThreadGuard>(this->thread->get_id());
}

Controller::~Controller()
{
    assert(this->quitting && "Application should call setQuitting() before "
                             "destroying the controller");

    qCInfo(LOG) << "Controller dtor start";

    for (const auto &weakConnection : this->connections)
    {
        auto connection = weakConnection.lock();
        if (!connection)
        {
            continue;
        }

        connection->close();
    }

    {
        std::lock_guard lock(this->subscriptionsMutex);
        this->subscriptions.clear();
    }

    this->work.reset();

    if (!this->thread->joinable())
    {
        qCInfo(LOG) << "Controller dtor end (not joinable)";
        return;
    }

    if (this->stoppedFlag.waitFor(250ms))
    {
        this->thread->join();
        qCInfo(LOG) << "Controller dtor end (joined)";
        return;
    }

    qCWarning(LOG) << "Controller dtor end (stopped flag didn't stop)";
}

void Controller::removeRef(const SubscriptionRequest &request)
{
    if (this->quitting)
    {
        // We're quitting - we don't care to unsub
        return;
    }

    std::lock_guard lock(this->subscriptionsMutex);

    assert(this->subscriptions.contains(request));

    auto &subscription = this->subscriptions[request];
    subscription.refCount--;
    assert(subscription.refCount >= 0);
    if (subscription.refCount == 0)
    {
        qCDebug(LOG) << "Removed last ref for" << request;
    }
    else
    {
        qCDebug(LOG) << "Removed ref for" << request << "("
                     << subscription.refCount << "remaining)";
    }

    if (subscription.refCount <= 0)
    {
        // No longer interested in this topic, ensure we don't have a retry in flight
        subscription.retryTimer.reset();
        if (subscription.subscriptionID.isEmpty())
        {
            qCDebug(LOG)
                << "Refcount fell to zero for" << request
                << "but we had no subscription ID attached - a "
                   "successful subscription was never made. From state "
                << qmagicenum::enumName(subscription.state);
            subscription.state = Subscription::State::Unsubscribed;
            subscription.backoff.reset();
            this->subscriptions.erase(request);
            return;
        }

        qCDebug(LOG) << "Unsubscribing from" << request;
        subscription.state = Subscription::State::Unsubscribing;

        getHelix()->deleteEventSubSubscription(
            subscription.subscriptionID,
            [this, request] {
                qCDebug(LOG) << "Successfully unsubscribed from" << request;
                this->markRequestUnsubscribed(request);
            },
            [this, request](const auto &errorMessage) {
                qCWarning(LOG)
                    << "An error occurred while attempting to unsubscribe from"
                    << request << errorMessage;
                this->markRequestUnsubscribed(request);
            });

        subscription.subscriptionID.clear();
    }
}

void Controller::setQuitting()
{
    this->quitting = true;
}

SubscriptionHandle Controller::subscribe(const SubscriptionRequest &request)
{
    assert(!this->quitting &&
           "Subscribe cannot be called while we are quitting");

    assert(!request.ownerTwitchUserID.isEmpty() &&
           "Subscription requests must include a Twitch User ID");

    // satorino: raid subscription requests originate on the GUI thread. Never
    // read mutable account objects later from the Asio worker.
    QString raidClient;
    QString raidToken;
    if (request.subscriptionType == "channel.raid")
    {
        assertInGuiThread();
        const auto account = getApp()->getAccounts()->twitch.getCurrent();
        if (!account || account->isAnon() ||
            account->getUserId() != request.ownerTwitchUserID)
            return {};
        raidClient = account->getOAuthClient();
        raidToken = account->getOAuthToken();
    }
    bool needToSubscribe = false;
    std::shared_ptr<void>
        raidGeneration;  // satorino: no allocation for other topics

    {
        // TODO: Investigate if this scope can be done in boost::asio::post instead
        // Basically, if the SubscriptionHandle can be built & returned entirely without waiting for the subscriptionsMutex lock
        std::lock_guard lock(this->subscriptionsMutex);

        auto &subscription = this->subscriptions[request];

        assert(subscription.refCount >= 0);

        switch (subscription.state)
        {
            case Subscription::State::Unsubscribed:
                needToSubscribe = true;
                assert(subscription.refCount == 0 &&
                       "An unsubscribed subscription should have 0 references");
                break;

            case Subscription::State::Failed:
                qCDebug(LOG)
                    << "New subscription attempt to previously-failed request"
                    << request;
                needToSubscribe = true;
                break;

            case Subscription::State::Subscribing:
            case Subscription::State::Retrying:
            case Subscription::State::Subscribed:
            case Subscription::State::Unsubscribing:
                break;
        }

        if (needToSubscribe)
        {
            qCDebug(LOG) << "Set state to subscribing" << request;
            subscription.state = Subscription::State::Subscribing;

            // Ensure retries can work as expected since this is a fresh subscription
            subscription.backoff.reset();

            assert(subscription.retryTimer == nullptr &&
                   "A new subscription should not have a retry timer created");
        }

        // satorino: retain a stable generation through retries and remove/re-add races.
        if (request.subscriptionType == "channel.raid")
        {
            if (!subscription.raidGeneration)
                subscription.raidGeneration = std::make_shared<int>(0);
            raidGeneration = subscription.raidGeneration;
            subscription.raidClient = raidClient;
            subscription.raidToken = raidToken;
        }
        subscription.refCount++;
        qCDebug(LOG) << "Added ref for" << request << subscription.refCount
                     << needToSubscribe
                     << "state:" << qmagicenum::enumName(subscription.state);
    }

    auto handle = std::make_unique<RawSubscriptionHandle>(request);

    if (needToSubscribe)
    {
        boost::asio::post(this->ioContext, [this, request, raidGeneration] {
            this->subscribe(request, false, raidGeneration);
        });
    }

    return handle;
}

void Controller::reconnectConnection(
    std::unique_ptr<lib::Listener> connection,
    const std::optional<std::string> &reconnectURL,
    const std::unordered_set<SubscriptionRequest> &subs)
{
    this->clearConnections();
    if (subs.empty())
    {
        return;
    }

    if (reconnectURL)
    {
        qCDebug(chatterinoTwitchEventSub) << "Using reconnect URL to reconnect";
        // this is epic
        QUrl url(QString::fromStdString(*reconnectURL));
        const auto replacement = this->createConnection(
            url.host(QUrl::FullyEncoded).toStdString(),
            std::to_string(url.port(443)),
            url.path(QUrl::FullyEncoded).toStdString(), std::move(connection));
        // satorino: reconnect preserves subscriptions; move raid weak references to the new session.
        std::lock_guard lock(this->subscriptionsMutex);
        for (const auto &request : subs)
        {
            const auto it = this->subscriptions.find(request);
            if (request.subscriptionType == "channel.raid" &&
                it != this->subscriptions.end() && it->second.refCount > 0)
                it->second.connection = replacement;
        }
        return;
    }

    // no reconnect URL - something happened
    // but first, clear the subscriptions
    qCDebug(chatterinoTwitchEventSub)
        << "Resubscribing to topics after connection failure";
    {
        std::lock_guard g(this->subscriptionsMutex);
        for (const auto &sub : subs)
        {
            auto it = this->subscriptions.find(sub);
            if (it != this->subscriptions.end())
            {
                qCDebug(chatterinoTwitchEventSub) << "Resetting" << it->first;
                it->second.connection = {};
                it->second.backoff.reset();
                it->second.state = Subscription::State::Subscribing;
            }
        }
    }

    for (const auto &sub : subs)
    {
        this->subscribe(sub, false);
    }
}

// satorino: a revoked raid topic must become retryable instead of remaining 'subscribed'.
void Controller::raidSubscriptionRevoked(const QString &id,
                                         const QString &reason)
{
    if (id.isEmpty())
        return;  // satorino: invalid revocation cannot match a pending POST
    std::lock_guard lock(this->subscriptionsMutex);
    for (auto &[request, sub] : this->subscriptions)
    {
        if (request.subscriptionType != "channel.raid" ||
            sub.subscriptionID != id || sub.refCount <= 0 ||
            sub.state != Subscription::State::Subscribed)
            continue;
        sub.raidError =
            "Twitch revoked subscription: " + satorino::raidSafeReason(reason);
        if (auto connection = sub.connection.lock())
            if (auto *listener =
                    dynamic_cast<Connection *>(connection->getListener()))
                listener->markRequestUnsubscribed(request);
        sub.connection.reset();
        sub.subscriptionID.clear();
        sub.retryTimer.reset();
        sub.state = Subscription::State::Failed;
        qCWarning(LOG) << "Raid watch revoked" << request << sub.raidError;
        return;
    }
}
// satorino: Twitch can deliver the first notification before its POST response.
QString Controller::raidPendingOwner(const QString &session,
                                     const QString &source) const
{
    if (session.isEmpty() || source.isEmpty())
        return {};
    std::lock_guard lock(this->subscriptionsMutex);
    for (const auto &[request, sub] : this->subscriptions)
        if (request.subscriptionType == "channel.raid" && sub.refCount > 0 &&
            sub.raidSessionID == session &&
            (sub.state == Subscription::State::Subscribing ||
             sub.state == Subscription::State::Retrying ||
             sub.state == Subscription::State::Subscribed))
            for (const auto &[key, value] : request.conditions)
                if (key == "from_broadcaster_user_id" && value == source)
                    return request.ownerTwitchUserID;
    return {};
}
// satorino: GUI reads the actual request state, not whether it owns a handle.
satorino::RaidSubscriptionStatus Controller::raidSubscriptionStatus(
    const SubscriptionRequest &request) const
{
    using State = satorino::RaidWatchState;
    std::lock_guard lock(this->subscriptionsMutex);
    const auto it = this->subscriptions.find(request);
    if (it == this->subscriptions.end())
        return {};
    const auto &sub = it->second;
    switch (sub.state)
    {
        case Subscription::State::Subscribed:
            return sub.connection.expired()
                       ? satorino::RaidSubscriptionStatus{State::Failed,
                                                          "connection lost"}
                       : satorino::RaidSubscriptionStatus{State::Subscribed,
                                                          {}};
        case Subscription::State::Subscribing:
            return {State::Subscribing, {}};
        case Subscription::State::Retrying:
            return {State::Waiting, sub.raidError};
        case Subscription::State::Failed:
            return {State::Failed, sub.raidError};
        default:
            return {};
    }
}
// satorino: lifetime-checked and sanitized before crossing into diagnostic state.
void Controller::setRaidError(const SubscriptionRequest &request,
                              const std::shared_ptr<void> &generation,
                              const QString &error)
{
    std::lock_guard lock(this->subscriptionsMutex);
    const auto it = this->subscriptions.find(request);
    if (it == this->subscriptions.end() ||
        it->second.raidGeneration != generation || it->second.refCount == 0)
        return;
    if (it->second.raidError != error)
        qCWarning(LOG) << "Raid watch could not subscribe" << request << error;
    it->second.raidError = error;
}
void Controller::debug()
{
    std::lock_guard g(this->subscriptionsMutex);
    for (const auto &[request, subscription] : this->subscriptions)
    {
        QString sessionID;
        auto connection = subscription.connection.lock();
        if (connection)
        {
            auto *connection2 =
                dynamic_cast<Connection *>(connection->getListener());
            if (connection2)
            {
                sessionID = connection2->getSessionID();
            }
            else
            {
                sessionID = "BAD";
            }
        }
        else
        {
            sessionID = "DEAD";
        }

        qCInfo(LOG).noquote().nospace()
            << request << " (" << qmagicenum::enumName(subscription.state)
            << ") -> " << sessionID;
    }

    boost::asio::post(this->ioContext, [this] {
        for (const auto &weakConnection : this->connections)
        {
            auto connection = weakConnection.lock();
            if (connection)
            {
                auto *connection2 =
                    dynamic_cast<Connection *>(connection->getListener());
                if (connection2)
                {
                    // qCInfo(LOG)
                    //     << "Connected to" << connection2->getSessionID();
                    connection2->debug();
                }
            }
            else
            {
                qCInfo(LOG) << "Dead connection";
            }
        }
    });
}

void Controller::subscribe(const SubscriptionRequest &request, bool isRetry,
                           const std::shared_ptr<void> &expectedRaidGeneration)
{
    std::shared_ptr<void> raidGeneration;  // satorino: this request lifetime
    QString raidClient;
    QString raidToken;
    // 1. Flush dead connections (maybe this should not be done here)
    // TODO: implement

    {
        std::lock_guard lock(this->subscriptionsMutex);
        // satorino: cancelled queued requests must not resurrect a raid subscription.
        if (request.subscriptionType == "channel.raid")
        {
            const auto it = this->subscriptions.find(request);
            if (it == this->subscriptions.end() || it->second.refCount == 0 ||
                (expectedRaidGeneration &&
                 it->second.raidGeneration != expectedRaidGeneration))
                return;
            raidGeneration = it->second.raidGeneration;
            raidClient = it->second.raidClient;
            raidToken = it->second.raidToken;
        }
        auto &subscription = this->subscriptions[request];
        if (isRetry)
        {
            qCDebug(LOG) << "Retry subscribe request for" << request;

            assert(subscription.retryTimer != nullptr);

            subscription.retryTimer.reset();
        }
        else
        {
            qCDebug(LOG) << "New subscribe request for" << request;
        }

        assert(subscription.retryTimer == nullptr);
    }

    uint32_t openButNotReadyConnections = 0;

    // 2. Check if any currently open connection can handle this subscription
    auto viableConnection =
        this->getViableConnection(request, openButNotReadyConnections);

    if (viableConnection.has_value())
    {
        const auto &connection = *viableConnection;
        auto *listener = dynamic_cast<Connection *>(connection->getListener());

        assert(listener != nullptr && "Something goofy has gone wrong, Session "
                                      "listener must be our Connection type");

        listener->claimHelixAuthMode(!request.helixOAuthToken.isEmpty());

        // satorino: register pending ownership before Twitch can send its first event.
        if (raidGeneration)
        {
            std::lock_guard lock(this->subscriptionsMutex);
            const auto it = this->subscriptions.find(request);
            if (it == this->subscriptions.end() ||
                it->second.raidGeneration != raidGeneration ||
                it->second.refCount == 0)
                return;
            it->second.raidSessionID = listener->getSessionID();
        }
        // satorino: use the immutable normal-account snapshot captured under the map lock.
        qCDebug(LOG) << "Make helix request for" << request;
        getHelix()->createEventSubSubscription(
            request, listener->getSessionID(),
            [this, request, raidGeneration, raidClient, raidToken,
             weakConnection{std::weak_ptr<lib::Session>(connection)}](
                const auto &res) {
                qCDebug(LOG) << "Subscription success" << request;
                // satorino: rejected late raid successes cannot leave an orphan subscription.
                const bool accepted = this->markRequestSubscribed(
                    request, weakConnection, res.subscriptionID,
                    raidGeneration);
                if (raidGeneration && !accepted)
                    deleteLateRaid(res.subscriptionID, raidClient, raidToken);
            },
            [this, request, raidGeneration, raidToken](
                const auto &error, const auto &rawErrorString) {
                // satorino: errors for raid watches are safe to show without reading a log.
                const QString errorString =
                    raidGeneration
                        ? satorino::raidSafeReason(rawErrorString, {raidToken})
                        : rawErrorString;
                if (raidGeneration)
                    this->setRaidError(request, raidGeneration, errorString);
                using Error = HelixCreateEventSubSubscriptionError;

                bool retry = false;
                switch (error)
                {
                    case Error::BadRequest:
                        qCDebug(LOG) << "Bad request" << errorString << request;
                        break;

                    case Error::Unauthorized:
                        qCDebug(LOG)
                            << "Unauthorized" << errorString << request;
                        break;

                    case Error::Forbidden:
                        qCDebug(LOG) << "Forbidden" << errorString << request;
                        break;

                    case Error::Conflict:
                        // This session ID is already subscribed to this request, some logic of ours is wrong
                        qCWarning(LOG) << "Conflict" << errorString << request;
                        break;

                    case Error::Ratelimited:
                        qCDebug(LOG) << "Ratelimited" << errorString << request;
                        break;

                    case Error::NoSession:
                        qCDebug(LOG) << "Session expired, retrying"
                                     << errorString << request;
                        retry = true;
                        break;

                    case Error::Forwarded:
                    default:
                        qCWarning(LOG) << "Unhandled error, retrying "
                                          "subscription"
                                       << errorString << request;
                        retry = true;
                        break;
                }

                if (retry)
                {
                    boost::asio::post(
                        this->ioContext, [this, request, raidGeneration] {
                            this->retrySubscription(request, raidGeneration);
                        });
                }
                else
                {
                    this->markRequestFailed(request, raidGeneration);
                }
            },
            raidGeneration ? raidClient : request.helixClientId,
            raidGeneration
                ? raidToken
                : request
                      .helixOAuthToken);  // satorino: same normal auth snapshot

        return;
    }

    if (openButNotReadyConnections == 0)
    {
        // No connection was available to handle this subscription request, create a new connection
        this->createConnection(!request.helixOAuthToken.isEmpty());
    }
    else if (openButNotReadyConnections > 1)
    {
        // There should only ever be 0 or 1
        qCWarning(LOG) << "We have" << openButNotReadyConnections
                       << "open but no ready connections";
    }

    this->retrySubscription(request, raidGeneration);
}

std::optional<std::shared_ptr<lib::Session>> Controller::getViableConnection(
    const SubscriptionRequest &request, uint32_t &openButNotReadyConnections)
{
    for (const auto &weakConnection : this->connections)
    {
        auto connection = weakConnection.lock();
        if (!connection)
        {
            // TODO: remove it here?
            continue;
        }

        auto *listener = dynamic_cast<Connection *>(connection->getListener());

        if (!listener)
        {
            continue;  // dead connection
        }

        if (!listener->canHandleSubscription(request))
        {
            continue;
        }

        if (listener->getSessionID().isEmpty())
        {
            // This connection is open but it's not ready (i.e. no welcome has been posted yet)
            ++openButNotReadyConnections;
            continue;
        }

        // TODO: Check if this listener has room for another subscription

        return connection;
    }

    return {};
}

void Controller::createConnection(bool alternateHelixAuth)
{
    auto listener = std::make_unique<Connection>();
    listener->claimHelixAuthMode(alternateHelixAuth);
    this->createConnection(this->eventSubHost, this->eventSubPort,
                           this->eventSubPath, std::move(listener));
}

std::shared_ptr<lib::Session> Controller::createConnection(
    std::string host, std::string port, std::string path,
    std::unique_ptr<lib::Listener> listener)
{
    qCDebug(LOG) << "Create EventSub connection";

    try
    {
        boost::asio::ssl::context sslContext{
            boost::asio::ssl::context::tlsv12_client};

#ifndef NDEBUG
        if (!getApp()->getArgs().useLocalEventsub)
#endif
        {
            sslContext.set_verify_mode(
                boost::asio::ssl::verify_peer |
                boost::asio::ssl::verify_fail_if_no_peer_cert);
            sslContext.set_default_verify_paths();

            boost::certify::enable_native_https_server_verification(sslContext);
        }

        auto connection = std::make_shared<lib::Session>(
            this->ioContext, sslContext, std::move(listener), this->logProxy);

        this->registerConnection(connection);

        connection->run(std::move(host), std::move(port), std::move(path),
                        this->userAgent);
        return connection;  // satorino: reconnect snapshot ownership
    }
    catch (std::exception &e)
    {
        qCWarning(LOG) << "Error in EventSub run thread" << e.what();
    }
    return {};  // satorino: observable failed replacement
}

void Controller::registerConnection(std::weak_ptr<lib::Session> &&connection)
{
    this->threadGuard->guard();

    this->connections.emplace_back(std::move(connection));
}

void Controller::retrySubscription(const SubscriptionRequest &request,
                                   const std::shared_ptr<void> &raidGeneration)
{
    if (isAppAboutToQuit())
    {
        qCDebug(LOG) << "retrySubscription, but app is quitting" << request;
        return;
    }

    std::lock_guard lock(this->subscriptionsMutex);

    // satorino: ignore late errors/timers from a released or replaced raid lifetime.
    if (request.subscriptionType == "channel.raid")
    {
        const auto it = this->subscriptions.find(request);
        if (it == this->subscriptions.end() || it->second.refCount == 0 ||
            it->second.raidGeneration != raidGeneration)
            return;
    }
    auto &subscription = this->subscriptions[request];

    if (subscription.refCount == 0)
    {
        qCDebug(LOG) << "No one is interested in this subscription anymore, "
                        "stop trying"
                     << request << "from state"
                     << qmagicenum::enumName(subscription.state);
        qCDebug(LOG) << "Set state to unsubscribed" << request;
        subscription.state = Subscription::State::Unsubscribed;

        this->subscriptions.erase(request);

        return;
    }

    if (subscription.state != Subscription::State::Retrying)
    {
        qCDebug(LOG) << "Set state to retrying" << request;
        subscription.state = Subscription::State::Retrying;
    }

    // we don't need a strong RNG here
    // NOLINTNEXTLINE(cert-*)
    std::chrono::milliseconds jitter{std::rand() % 256};

    auto retryTimer =
        std::make_unique<boost::asio::system_timer>(this->ioContext);
    retryTimer->expires_after(subscription.backoff.next() + jitter);
    retryTimer->async_wait([this, request, raidGeneration](const auto &ec) {
        if (isAppAboutToQuit())
        {
            qCDebug(LOG)
                << "Retry was going to fire, but app is quitting so we won't."
                << request;
            return;
        }

        if (!ec)
        {
            qCDebug(LOG) << "Firing retry" << request;
            // The timer passed naturally
            this->subscribe(request, true, raidGeneration);
        }
        else
        {
            qCDebug(LOG) << "Retry timer for" << request << "was cancelled";
            // If we mark the request as unsubscribed here, and we had to actually unsubscribe,
            // if an actual unsubscribe happens then it'll go from Unsubscribed -> ACTUALLY unsubscribed
            //
            // We might still need to update the state here, but we might want some new state for that
            // e.g. RetryCancelled or something
            // this->markRequestUnsubscribed(request);
        }
    });

    assert(subscription.retryTimer == nullptr &&
           "Timer should not already be set");

    subscription.retryTimer = std::move(retryTimer);
}

bool Controller::markRequestSubscribed(
    const SubscriptionRequest &request, std::weak_ptr<lib::Session> connection,
    const QString &subscriptionID, const std::shared_ptr<void> &raidGeneration)
{
    if (this->quitting)
    {
        return false;
    }

    std::lock_guard lock(this->subscriptionsMutex);

    auto it = this->subscriptions.find(request);
    if (it == this->subscriptions.end())
    {
        return false;
    }

    // satorino: a new handle with the same source is a different request lifetime.
    if (request.subscriptionType == "channel.raid" &&
        (it->second.refCount == 0 ||
         it->second.raidGeneration != raidGeneration))
        return false;
    auto &subscription = it->second;

    if (subscription.state == Subscription::State::Subscribed)
    {
        return subscription.subscriptionID ==
               subscriptionID;  // satorino: idempotent response
    }

    if (subscription.state != Subscription::State::Subscribing &&
        subscription.state != Subscription::State::Retrying)
    {
        return false;
    }

    auto strong = connection.lock();
    if (!strong)
    {
        // we disconnected
        return false;
    }
    auto *listener = dynamic_cast<Connection *>(strong->getListener());
    if (!listener)
    {
        // we disconnected
        return false;
    }
    listener->markRequestSubscribed(request);
    if (raidGeneration)
    {
        subscription.raidError.clear();
        qCInfo(LOG) << "Raid watch subscribed" << request;  // satorino
    }

    subscription.connection = std::move(connection);
    subscription.subscriptionID = subscriptionID;
    qCDebug(LOG) << "Set state to subscribed" << request;
    subscription.state = Subscription::State::Subscribed;
    subscription.backoff.reset();
    return true;
}

void Controller::markRequestFailed(const SubscriptionRequest &request,
                                   const std::shared_ptr<void> &raidGeneration)
{
    if (this->quitting)
    {
        return;
    }

    qCWarning(LOG) << "Request" << request << "marked as failed";

    std::lock_guard lock(this->subscriptionsMutex);

    // satorino: a cancelled raid error must not create a new map entry.
    if (request.subscriptionType == "channel.raid")
    {
        const auto it = this->subscriptions.find(request);
        if (it == this->subscriptions.end() || it->second.refCount == 0 ||
            it->second.raidGeneration != raidGeneration)
            return;
    }
    auto &subscription = this->subscriptions[request];

    qCDebug(LOG) << "Set state to failed" << request;
    subscription.state = Subscription::State::Failed;
}

void Controller::markRequestUnsubscribed(const SubscriptionRequest &request)
{
    if (this->quitting)
    {
        return;
    }

    std::lock_guard lock(this->subscriptionsMutex);

    auto &subscription = this->subscriptions[request];

    qCDebug(LOG) << "Request" << request << "marked as unsubscribed from state"
                 << qmagicenum::enumName(subscription.state);

    assert(subscription.state == Subscription::State::Unsubscribing ||
           subscription.state == Subscription::State::Retrying);
    assert(subscription.retryTimer == nullptr);

    qCDebug(LOG) << "Set state to unsubscribed" << request;
    subscription.state = Subscription::State::Unsubscribed;
    subscription.backoff.reset();
    auto conn = subscription.connection.lock();
    if (conn)
    {
        auto *listener = dynamic_cast<Connection *>(conn->getListener());
        if (listener)
        {
            listener->markRequestUnsubscribed(request);
        }
    }
    subscription.connection = {};

    if (subscription.refCount == 0)
    {
        // we could remove the subscription here
        this->subscriptions.erase(request);
        return;
    }

    // someone subscribed in the meantime
    subscription.state = Subscription::State::Subscribing;
    boost::asio::post(this->ioContext, [this, request] {
        this->subscribe(request, false);
    });
}

void Controller::clearConnections()
{
    std::erase_if(this->connections, [](const auto &it) {
        auto conn = it.lock();
        return !conn || !conn->getListener();
    });
}

void DummyController::reconnectConnection(
    std::unique_ptr<lib::Listener> /* connection */,
    const std::optional<std::string> & /* reconnectURL */,
    const std::unordered_set<SubscriptionRequest> & /* subs */)
{
}

}  // namespace chatterino::eventsub
