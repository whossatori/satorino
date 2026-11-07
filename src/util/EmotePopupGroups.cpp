// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/EmotePopupGroups.hpp"

namespace chatterino::satorino {

const std::array<QString, 2> &sevenTvPopupEmoteLabels()
{
    using namespace Qt::StringLiterals;
    static const std::array<QString, 2> labels{u"Zero-width emotes"_s,
                                               u"Emotes"_s};
    return labels;
}

std::size_t sevenTvPopupEmoteGroup(bool zeroWidth)
{
    return zeroWidth ? 0U : 1U;
}

// satorino: documented Get User Emotes types; unknown/restricted types stay other.
TwitchPopupEmoteCategory twitchPopupEmoteCategory(QStringView type)
{
    if (type == u"globals" || type == u"smilies")
    {
        return TwitchPopupEmoteCategory::Default;
    }
    if (type == u"hypetrain")
    {
        return TwitchPopupEmoteCategory::HypeTrain;
    }
    return TwitchPopupEmoteCategory::Other;
}

const std::array<QString, 3> &twitchPopupEmoteLabels()
{
    using namespace Qt::StringLiterals;
    // The API does not distinguish chest-only rewards from other Hype Train rewards.
    static const std::array<QString, 3> labels{
        u"Default emotes"_s, u"Hype Train emotes"_s, u"Other emotes"_s};
    return labels;
}

}  // namespace chatterino::satorino
