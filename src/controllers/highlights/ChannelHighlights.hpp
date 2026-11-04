// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/ChatterinoSetting.hpp"
#include "common/SignalVector.hpp"
#include "common/UniqueAccess.hpp"
#include "controllers/highlights/ChannelHighlightData.hpp"

#include <pajlada/signals/signalholder.hpp>

#include <map>
#include <memory>

namespace chatterino {
class Channel;
}

namespace chatterino::satorino {

// Empty for special/aggregate splits; literal views do not allocate.
QStringView channelHighlightPlatform(const Channel *channel);

class ChannelHighlights
{
public:
    ChannelHighlights();
    SignalVector<ChannelHighlightPhrase> &editable(ChannelHighlightKeyView key);
    HighlightEvaluation apply(const HighlightCheckParams &params,
                              HighlightEvaluation global) const;

private:
    struct Editor {
        SignalVector<ChannelHighlightPhrase> phrases;
        pajlada::Signals::SignalHolder connections;
    };

    void reload();
    void persist(const QString &key, const Editor &editor);

    ChatterinoSetting<ChannelHighlightRules> rules_{
        "/satorino/highlights/channels", ChannelHighlightRules{}};
    UniqueAccess<ChannelHighlightIndex> index_;
    std::map<QString, std::unique_ptr<Editor>> editors_;
    bool synchronizing_ = false;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino::satorino
