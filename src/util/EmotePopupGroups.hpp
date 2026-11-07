// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringView>

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace chatterino::satorino {

// satorino: one production order/classifier shared with the popup regression tests.
const std::array<QString, 2> &sevenTvPopupEmoteLabels();
std::size_t sevenTvPopupEmoteGroup(bool zeroWidth);

// satorino: display metadata only; no changes to emote parsing or availability.
enum class TwitchPopupEmoteCategory : std::size_t {
    Default,
    HypeTrain,
    Other,
};

TwitchPopupEmoteCategory twitchPopupEmoteCategory(QStringView type);
const std::array<QString, 3> &twitchPopupEmoteLabels();

// satorino: popup-only stable partition; no widgets, sorting or source mutation.
template <typename T>
struct EmotePopupGroup {
    QString title;
    std::vector<T> emotes;
};

// The classifier returns an index into labels. Filtering happens before grouping,
// so empty subsections disappear without changing label or surviving-item order.
template <typename T, std::size_t GroupCount, typename Classifier,
          typename Matcher>
std::vector<EmotePopupGroup<T>> groupEmotePopup(
    const std::vector<T> &emotes, const std::array<QString, GroupCount> &labels,
    Classifier classify, Matcher matches)
{
    static_assert(GroupCount > 0);
    std::array<std::vector<T>, GroupCount> buckets{};
    for (const auto &emote : emotes)
    {
        if (matches(emote))
        {
            buckets.at(static_cast<std::size_t>(classify(emote)))
                .push_back(emote);
        }
    }

    std::vector<EmotePopupGroup<T>> groups;
    for (std::size_t i = 0; i < GroupCount; ++i)
    {
        if (!buckets[i].empty())
        {
            groups.push_back({labels[i], std::move(buckets[i])});
        }
    }
    return groups;
}

}  // namespace chatterino::satorino
