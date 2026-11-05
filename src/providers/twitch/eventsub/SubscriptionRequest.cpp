// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/eventsub/SubscriptionRequest.hpp"

#include <QDebug>

namespace chatterino::eventsub {

QDebug operator<<(QDebug dbg, const SubscriptionRequest &v)
{
    QDebugStateSaver saver(dbg);

    dbg.nospace().noquote()
        << "eventsub::SubscriptionRequest[" << v.subscriptionType << '.'
        << v.subscriptionVersion << "]{";
    bool first = true;
    if (!v.conditions.empty())
    {
        for (const auto &[conditionKey, conditionValue] : v.conditions)
        {
            if (!first)
            {
                dbg.nospace() << ", ";
            }
            dbg.nospace().noquote() << conditionKey << "=" << conditionValue;

            first = false;
        }
    }
    dbg.nospace() << "} ";

    return dbg;
}

bool operator==(const SubscriptionRequest &lhs, const SubscriptionRequest &rhs)
{
    // satorino: raid ownership stays distinct during an account-change unsubscribe.
    if (lhs.subscriptionType == "channel.raid" &&
        lhs.ownerTwitchUserID != rhs.ownerTwitchUserID)
        return false;
    return std::tie(lhs.subscriptionType, lhs.subscriptionVersion,
                    lhs.conditions) == std::tie(rhs.subscriptionType,
                                                rhs.subscriptionVersion,
                                                rhs.conditions);
}

bool operator!=(const SubscriptionRequest &lhs, const SubscriptionRequest &rhs)
{
    return !(lhs == rhs);
}

}  // namespace chatterino::eventsub
