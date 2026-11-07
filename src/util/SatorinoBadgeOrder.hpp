// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <algorithm>
#include <vector>

namespace chatterino::satorino {

struct BadgeKindInfo {
    QString key;
    QString label;
};

const std::vector<BadgeKindInfo> &badgeKinds();
QStringList defaultBadgeKindOrder();
QStringList normalizeBadgeKindOrder(const QStringList &saved);

// Empty/entirely invalid saved order means the exact original element sequence.
struct BadgeOrderRanks {
    bool custom = false;
    QHash<QString, int> ranks;
    int rank(const QString &kind) const;
};
BadgeOrderRanks makeBadgeOrderRanks(const QStringList &saved);
// satorino: this setting only changes Twitch badge layout.
bool badgeOrderApplies(QStringView scope, bool isOwnMessage, bool isTwitch);

enum class BadgeIconChoice { Real, Placeholder };
// Inputs describe the final decoded/scaled pixels, never URL metadata.
BadgeIconChoice badgeIconChoice(bool hasVisiblePixels, int width, int height);
bool badgeIconInPreview(BadgeIconChoice choice);
bool badgeOrderOwnAuthor(QStringView author, QStringView current,
                         bool loggedIn);
QString twitchBadgeOrderKind(QStringView slug);

// satorino: callers pass only the few badges, never re-order text or mutate Messages.
template <typename T, typename KindFor>
std::vector<T> stableBadgeOrder(std::vector<T> badges,
                                const BadgeOrderRanks &ranks, KindFor kindFor)
{
    if (ranks.custom)
    {
        std::stable_sort(
            badges.begin(), badges.end(), [&](const T &a, const T &b) {
                return ranks.rank(kindFor(a)) < ranks.rank(kindFor(b));
            });
    }
    return badges;
}

}  // namespace chatterino::satorino
