// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>
#include <QStringView>

#include <array>
#include <cstdint>

namespace chatterino {

enum class EmoteProvider : uint8_t {
    Twitch,
    Ffz,
    Bttv,
    SevenTV,
};

using EmotePriority = std::array<EmoteProvider, 4>;

inline constexpr EmotePriority DEFAULT_EMOTE_PRIORITY{
    EmoteProvider::Twitch,
    EmoteProvider::Ffz,
    EmoteProvider::Bttv,
    EmoteProvider::SevenTV,
};

QStringView emoteProviderName(EmoteProvider provider);
EmotePriority parseEmotePriority(const QStringList &saved);

template <typename Lookup>
auto firstEmote(const EmotePriority &priority, Lookup lookup)
{
    for (const auto provider : priority)
    {
        if (auto emote = lookup(provider))
        {
            return emote;
        }
    }
    return decltype(lookup(EmoteProvider::Twitch)){};
}

}  // namespace chatterino
