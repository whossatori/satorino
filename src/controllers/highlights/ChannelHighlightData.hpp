// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/highlights/HighlightCheck.hpp"
#include "controllers/highlights/HighlightResult.hpp"
#include "controllers/highlights/Legacy.hpp"

#include <QStringView>

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chatterino::satorino {

// Retain the existing phrase schema and the independent sound/custom-file flags.
struct ChannelHighlightPhrase : highlights::legacy::HighlightPhrase {
    static ChannelHighlightPhrase createDefault();
    bool operator==(const ChannelHighlightPhrase &other) const;
};

using ChannelHighlightRules =
    std::map<std::string, std::vector<ChannelHighlightPhrase>>;
using HighlightEvaluation = std::pair<bool, HighlightResult>;

struct ChannelHighlightKeyView {
    QStringView platform;
    QStringView channel;
};

QString channelHighlightKey(ChannelHighlightKeyView key);

struct ChannelHighlightKeyHash {
    using is_transparent = void;
    std::size_t operator()(const QString &key) const;
    std::size_t operator()(ChannelHighlightKeyView key) const;
};

struct ChannelHighlightKeyEqual {
    using is_transparent = void;
    bool operator()(const QString &left, const QString &right) const;
    bool operator()(const QString &left, ChannelHighlightKeyView right) const;
    bool operator()(ChannelHighlightKeyView left, const QString &right) const;
};

class ChannelHighlightIndex
{
public:
    void rebuild(const ChannelHighlightRules &rules);
    const std::vector<HighlightCheck> *find(ChannelHighlightKeyView key) const;

private:
    std::unordered_map<QString, std::vector<HighlightCheck>,
                       ChannelHighlightKeyHash, ChannelHighlightKeyEqual>
        checks_;
};

HighlightEvaluation evaluateChannelPhrases(
    const std::vector<HighlightCheck> &checks,
    const HighlightCheckParams &params);
HighlightEvaluation applyChannelHighlightPriority(
    HighlightEvaluation global, const HighlightEvaluation &local);

}  // namespace chatterino::satorino

namespace pajlada {

template <>
struct Serialize<chatterino::satorino::ChannelHighlightPhrase>
    : Serialize<chatterino::highlights::legacy::HighlightPhrase> {
};

template <>
struct Deserialize<chatterino::satorino::ChannelHighlightPhrase> {
    static chatterino::satorino::ChannelHighlightPhrase get(
        const rapidjson::Value &value, bool *error = nullptr)
    {
        chatterino::satorino::ChannelHighlightPhrase phrase;
        static_cast<chatterino::highlights::legacy::HighlightPhrase &>(phrase) =
            Deserialize<chatterino::highlights::legacy::HighlightPhrase>::get(
                value, error);
        return phrase;
    }
};

}  // namespace pajlada
