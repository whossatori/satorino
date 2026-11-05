// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "Test.hpp"
#include "twitch-eventsub-ws/listener.hpp"
#include "twitch-eventsub-ws/logger.hpp"
#include "twitch-eventsub-ws/session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/json.hpp>
#include <boost/system/error_code.hpp>
#include <QDateTime>
#include <QtGlobal>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace lib = chatterino::eventsub::lib;
namespace {
class RaidListener : public lib::Listener
{
public:
    int received = 0;
    bool payloadOnly = false;
    qint64 timestampMs = 0;
    std::string messageType;
    void onNotification(const lib::messages::Metadata &metadata,
                        const boost::json::value &value) override
    {
        ++this->received;
        const auto &object = value.as_object();
        this->payloadOnly =
            object.contains("subscription") && !object.contains("payload");
        this->timestampMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                metadata.messageTimestamp.time_since_epoch())
                .count();
        this->messageType = metadata.messageType;
    }
    void onSessionWelcome(
        const lib::messages::Metadata &,
        const lib::payload::session_welcome::Payload &) override
    {
    }
    void onChannelBan(const lib::messages::Metadata &,
                      const lib::payload::channel_ban::v1::Payload &) override
    {
    }
    void onStreamOnline(
        const lib::messages::Metadata &,
        const lib::payload::stream_online::v1::Payload &) override
    {
    }
    void onStreamOffline(
        const lib::messages::Metadata &,
        const lib::payload::stream_offline::v1::Payload &) override
    {
    }
    void onChannelChatNotification(
        const lib::messages::Metadata &,
        const lib::payload::channel_chat_notification::v1::Payload &) override
    {
    }
    void onChannelUpdate(
        const lib::messages::Metadata &,
        const lib::payload::channel_update::v1::Payload &) override
    {
    }
    void onChannelChatMessage(
        const lib::messages::Metadata &,
        const lib::payload::channel_chat_message::v1::Payload &) override
    {
    }
    void onChannelModerate(
        const lib::messages::Metadata &,
        const lib::payload::channel_moderate::v2::Payload &) override
    {
    }
    void onAutomodMessageHold(
        const lib::messages::Metadata &,
        const lib::payload::automod_message_hold::v2::Payload &) override
    {
    }
    void onAutomodMessageUpdate(
        const lib::messages::Metadata &,
        const lib::payload::automod_message_update::v2::Payload &) override
    {
    }
    void onChannelSuspiciousUserMessage(
        const lib::messages::Metadata &,
        const lib::payload::channel_suspicious_user_message::v1::Payload &)
        override
    {
    }
    void onChannelSuspiciousUserUpdate(
        const lib::messages::Metadata &,
        const lib::payload::channel_suspicious_user_update::v1::Payload &)
        override
    {
    }
    void onChannelChatUserMessageHold(
        const lib::messages::Metadata &,
        const lib::payload::channel_chat_user_message_hold::v1::Payload &)
        override
    {
    }
    void onChannelChatUserMessageUpdate(
        const lib::messages::Metadata &,
        const lib::payload::channel_chat_user_message_update::v1::Payload &)
        override
    {
    }
    void onChannelFollow(
        const lib::messages::Metadata &,
        const lib::payload::channel_follow::v2::Payload &) override
    {
    }
};
boost::system::error_code dispatch(lib::Session &session,
                                   const std::string &message)
{
    boost::beast::flat_buffer buffer;
    boost::asio::buffer_copy(buffer.prepare(message.size()),
                             boost::asio::buffer(message));
    buffer.commit(message.size());
    return session.handleMessage(buffer);
}
}  // namespace
TEST(SatorinoRaidDispatch,
     RawRaidAndRevocationAreConsumedWithoutTypedHandlerError)
{
    boost::asio::io_context io;
    boost::asio::ssl::context tls(boost::asio::ssl::context::tlsv12_client);
    auto listener = std::make_unique<RaidListener>();
    auto *observed = listener.get();
    auto session = std::make_shared<lib::Session>(
        io, tls, std::move(listener), std::make_shared<lib::NullLogger>());
    // No run()/socket connection: exercise the actual library dispatch entry offline.
    const std::string raid =
        R"({"metadata":{"message_id":"46c57fa1-4b3c-4326-a90f-00297a0ec8b0","message_type":"notification","message_timestamp":"2026-11-05T13:45:12.123456789Z","subscription_type":"channel.raid","subscription_version":"1"},"payload":{"subscription":{"type":"channel.raid","version":"1","condition":{"from_broadcaster_user_id":"100"}},"event":{"from_broadcaster_user_id":"100","to_broadcaster_user_id":"200","to_broadcaster_user_login":"alt"}}})";
    EXPECT_FALSE(dispatch(*session, raid));
    EXPECT_EQ(observed->received, 1);
    EXPECT_TRUE(observed->payloadOnly);
    EXPECT_EQ(
        observed->timestampMs,
        QDateTime::fromString("2026-11-05T13:45:12.123Z", Qt::ISODateWithMs)
            .toMSecsSinceEpoch());
    EXPECT_EQ(session->getListener(), observed);
    const std::string revoked =
        R"({"metadata":{"message_id":"3bd57fa1-4b3c-4326-a90f-00297a0ec8b0","message_type":"revocation","message_timestamp":"2026-11-05T13:46:12Z","subscription_type":"channel.raid","subscription_version":"1"},"payload":{"subscription":{"id":"raid-id","type":"channel.raid","version":"1","status":"authorization_revoked"}}})";
    EXPECT_FALSE(dispatch(*session, revoked));
    EXPECT_EQ(observed->received, 2);
    EXPECT_EQ(observed->messageType, "revocation");
    const std::string keepalive =
        R"({"metadata":{"message_id":"4bd57fa1-4b3c-4326-a90f-00297a0ec8b0","message_type":"session_keepalive","message_timestamp":"2026-11-05T13:47:12Z"},"payload":{}})";
    EXPECT_FALSE(dispatch(*session, keepalive));
    EXPECT_EQ(session->getListener(), observed);
}
