// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/emotes/EmotePriority.hpp"
#include "Test.hpp"

#include <memory>

using namespace chatterino;

TEST(EmotePriority, RepairsIncompleteOrInvalidSettings)
{
    using enum EmoteProvider;
    EXPECT_EQ(parseEmotePriority({}), DEFAULT_EMOTE_PRIORITY);
    EXPECT_EQ(parseEmotePriority({"7TV", "invalid", "7TV", "BTTV"}),
              (EmotePriority{SevenTV, Bttv, Twitch, Ffz}));
}

TEST(EmotePriority, SelectsFirstAvailableProviderIncludingTwitch)
{
    using enum EmoteProvider;
    const auto native = std::make_shared<int>(1);
    const auto ffz = std::make_shared<int>(2);
    const auto lookup = [&](EmoteProvider provider) -> std::shared_ptr<int> {
        if (provider == Twitch)
        {
            return native;
        }
        if (provider == Ffz)
        {
            return ffz;
        }
        return {};
    };
    EXPECT_EQ(firstEmote(DEFAULT_EMOTE_PRIORITY, lookup), native);
    EXPECT_EQ(firstEmote(parseEmotePriority({"7TV", "FFZ"}), lookup), ffz);
    EXPECT_FALSE(firstEmote(DEFAULT_EMOTE_PRIORITY, [](EmoteProvider) {
        return std::shared_ptr<int>{};
    }));
}
