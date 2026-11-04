// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/highlights/ChannelHighlights.hpp"

#include "common/Channel.hpp"
#include "debug/AssertInGuiThread.hpp"

namespace chatterino::satorino {

QStringView channelHighlightPlatform(const Channel *channel)
{
    if (!channel || channel->getName().isEmpty())
    {
        return {};
    }
    switch (channel->getType())
    {
        case Channel::Type::Twitch:
            return u"twitch";
        case Channel::Type::Kick:
            return u"kick";
        case Channel::Type::YouTube:
            return u"youtube";
        default:
            return {};
    }
}

ChannelHighlights::ChannelHighlights()
{
    this->rules_.connect(
        [this] {
            this->reload();
        },
        this->connections_);
}

void ChannelHighlights::reload()
{
    assertInGuiThread();
    const auto rules = this->rules_.getValueCopy();
    ChannelHighlightIndex replacement;
    replacement.rebuild(rules);
    // Compile outside the lock; readers only pause while swapping the index.
    {
        auto index = this->index_.access();
        *index = std::move(replacement);
    }

    this->synchronizing_ = true;
    for (const auto &[key, editor] : this->editors_)
    {
        const auto found = rules.find(key.toStdString());
        const std::vector<ChannelHighlightPhrase> empty;
        const auto &phrases = found == rules.end() ? empty : found->second;
        if (editor->phrases.raw() == phrases)
        {
            continue;
        }
        editor->phrases.clear();
        for (const auto &phrase : phrases)
        {
            editor->phrases.append(phrase);
        }
    }
    this->synchronizing_ = false;
}

SignalVector<ChannelHighlightPhrase> &ChannelHighlights::editable(
    ChannelHighlightKeyView key)
{
    assertInGuiThread();
    const auto name = channelHighlightKey(key);
    auto &editor = this->editors_[name];
    if (!editor)
    {
        editor = std::make_unique<Editor>();
        const auto rules = this->rules_.getValueCopy();
        if (auto it = rules.find(name.toStdString()); it != rules.end())
        {
            for (const auto &phrase : it->second)
            {
                editor->phrases.append(phrase);
            }
        }
        auto *entry = editor.get();
        const auto changed = [this, name, entry] {
            if (!this->synchronizing_)
            {
                this->persist(name, *entry);
            }
        };
        // SignalVector has already updated raw() at each synchronous event.
        // Do not wait for its 100ms delayed signal before publishing new checks.
        editor->connections.managedConnect(editor->phrases.itemInserted,
                                           [changed](auto) {
                                               changed();
                                           });
        editor->connections.managedConnect(editor->phrases.itemRemoved,
                                           [changed](auto) {
                                               changed();
                                           });
        editor->connections.managedConnect(editor->phrases.itemsCleared,
                                           changed);
    }
    return editor->phrases;
}

void ChannelHighlights::persist(const QString &key, const Editor &editor)
{
    auto rules = this->rules_.getValueCopy();
    if (editor.phrases.raw().empty())
    {
        rules.erase(key.toStdString());
    }
    else
    {
        rules[key.toStdString()] = editor.phrases.raw();
    }
    this->rules_.setValue(rules);
}

HighlightEvaluation ChannelHighlights::apply(const HighlightCheckParams &params,
                                             HighlightEvaluation global) const
{
    const auto *channel = params.runContext.channel;
    const auto platform = channelHighlightPlatform(channel);
    if (platform.isEmpty())
    {
        return global;
    }
    const auto index = this->index_.accessConst();
    const auto *checks = index->find({platform, channel->getName()});
    if (!checks)
    {
        return global;
    }
    return applyChannelHighlightPriority(
        std::move(global), evaluateChannelPhrases(*checks, params));
}

}  // namespace chatterino::satorino
