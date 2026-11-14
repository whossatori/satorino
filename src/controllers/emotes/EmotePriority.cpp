// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/emotes/EmotePriority.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

namespace chatterino {

QStringView emoteProviderName(EmoteProvider provider)
{
    switch (provider)
    {
        case EmoteProvider::Twitch:
            return u"Twitch";
        case EmoteProvider::Ffz:
            return u"FFZ";
        case EmoteProvider::Bttv:
            return u"BTTV";
        case EmoteProvider::SevenTV:
            return u"7TV";
    }
    return {};
}

EmotePriority parseEmotePriority(const QStringList &saved)
{
    EmotePriority priority{};
    std::size_t count = 0;
    const auto append = [&](EmoteProvider provider) {
        const auto used = std::span{priority}.first(count);
        if (std::ranges::find(used, provider) == used.end())
        {
            priority[count++] = provider;
        }
    };

    for (const auto &name : saved)
    {
        const auto provider = std::ranges::find_if(
            DEFAULT_EMOTE_PRIORITY, [&](EmoteProvider value) {
                return emoteProviderName(value) == name;
            });
        if (provider != DEFAULT_EMOTE_PRIORITY.end())
        {
            append(*provider);
        }
    }

    for (auto provider : DEFAULT_EMOTE_PRIORITY)
    {
        append(provider);
    }
    return priority;
}

}  // namespace chatterino
