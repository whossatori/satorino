// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/highlights/ChannelHighlightData.hpp"

#include "controllers/highlights/types/MessageHighlight.hpp"

#include <tuple>

namespace chatterino::satorino {
namespace {

QStringView channelName(QStringView name)
{
    name = name.trimmed();
    return name.startsWith(u'#') ? name.mid(1) : name;
}

void hashCharacter(std::size_t &hash, QChar ch)
{
    hash = (hash ^ ch.toLower().unicode()) * 16777619U;
}

bool equalFolded(QStringView left, QStringView right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (qsizetype i = 0; i < left.size(); ++i)
    {
        if (left[i].toLower() != right[i].toLower())
        {
            return false;
        }
    }
    return true;
}

HighlightCheck buildPhraseCheck(const ChannelHighlightPhrase &phrase)
{
    // Use the ACTIVE global phrase matcher, including word boundaries, regex
    // validation, Unicode/case options, and suppression of self messages.
    highlights::MessageHighlight h(u"satorino-channel");
    h.setPattern(phrase.pattern);
    h.setRegex(phrase.isRegex);
    h.setCaseSensitive(phrase.isCaseSensitive);
    h.outcome.showInMentions = phrase.showInMentions;
    h.outcome.alert = phrase.hasAlert;
    h.outcome.setBackgroundColor(phrase.color);
    h.outcome.setSound(!phrase.hasSound ? QStringLiteral("")
                       : phrase.soundUrl.isEmpty()
                           ? QStringLiteral("001-ping2")
                           : phrase.soundUrl.toString());
    return h.buildCheck();
}

}  // namespace

ChannelHighlightPhrase ChannelHighlightPhrase::createDefault()
{
    ChannelHighlightPhrase phrase;
    phrase.pattern = "my phrase";
    phrase.showInMentions =
        highlights::MessageHighlight::SHOW_IN_MENTIONS_DEFAULT;
    phrase.hasAlert = highlights::MessageHighlight::ALERT_DEFAULT;
    phrase.color = highlights::MessageHighlight::BACKGROUND_COLOR_DEFAULT;
    return phrase;
}

bool ChannelHighlightPhrase::operator==(
    const ChannelHighlightPhrase &other) const
{
    return std::tie(this->pattern, this->showInMentions, this->hasAlert,
                    this->hasSound, this->isRegex, this->isCaseSensitive,
                    this->soundUrl, this->color) ==
           std::tie(other.pattern, other.showInMentions, other.hasAlert,
                    other.hasSound, other.isRegex, other.isCaseSensitive,
                    other.soundUrl, other.color);
}

QString channelHighlightKey(ChannelHighlightKeyView key)
{
    const auto platform = key.platform.trimmed();
    const auto name = channelName(key.channel);
    if (platform.isEmpty() || name.isEmpty())
    {
        return {};
    }
    QString result;
    result.reserve(platform.size() + name.size() + 1);
    for (auto ch : platform)
    {
        result.append(ch.toLower());
    }
    result.append(u':');
    for (auto ch : name)
    {
        result.append(ch.toLower());
    }
    return result;
}

std::size_t ChannelHighlightKeyHash::operator()(const QString &key) const
{
    std::size_t hash = 2166136261U;
    for (auto ch : key)
    {
        hashCharacter(hash, ch);
    }
    return hash;
}

std::size_t ChannelHighlightKeyHash::operator()(
    ChannelHighlightKeyView key) const
{
    std::size_t hash = 2166136261U;
    for (auto ch : key.platform.trimmed())
    {
        hashCharacter(hash, ch);
    }
    hashCharacter(hash, u':');
    for (auto ch : channelName(key.channel))
    {
        hashCharacter(hash, ch);
    }
    return hash;
}

bool ChannelHighlightKeyEqual::operator()(const QString &left,
                                          const QString &right) const
{
    return equalFolded(left, right);
}

bool ChannelHighlightKeyEqual::operator()(const QString &left,
                                          ChannelHighlightKeyView right) const
{
    const auto separator = left.indexOf(u':');
    return separator >= 0 &&
           equalFolded(QStringView(left).left(separator),
                       right.platform.trimmed()) &&
           equalFolded(QStringView(left).mid(separator + 1),
                       channelName(right.channel));
}

bool ChannelHighlightKeyEqual::operator()(ChannelHighlightKeyView left,
                                          const QString &right) const
{
    return (*this)(right, left);
}

void ChannelHighlightIndex::rebuild(const ChannelHighlightRules &rules)
{
    this->checks_.clear();
    for (const auto &[storedKey, phrases] : rules)
    {
        if (phrases.empty())
        {
            continue;
        }
        const auto raw = QString::fromStdString(storedKey);
        const auto separator = raw.indexOf(u':');
        if (separator <= 0)
        {
            continue;
        }
        const auto key =
            channelHighlightKey({QStringView(raw).left(separator),
                                 QStringView(raw).mid(separator + 1)});
        if (key.isEmpty())
        {
            continue;
        }
        std::vector<HighlightCheck> checks;
        for (const auto &phrase : phrases)
        {
            if (auto check = buildPhraseCheck(phrase))
            {
                checks.push_back(std::move(check));
            }
        }
        if (!checks.empty())
        {
            this->checks_.insert_or_assign(key, std::move(checks));
        }
    }
}

const std::vector<HighlightCheck> *ChannelHighlightIndex::find(
    ChannelHighlightKeyView key) const
{
    // Heterogeneous lookup: no QString construction, concatenation, or allocation.
    const auto it = this->checks_.find(key);
    return it == this->checks_.end() ? nullptr : &it->second;
}

HighlightEvaluation evaluateChannelPhrases(
    const std::vector<HighlightCheck> &checks,
    const HighlightCheckParams &params)
{
    HighlightEvaluation result{false, HighlightResult::emptyResult()};
    for (const auto &check : checks)
    {
        if (const auto match = check(params))
        {
            result.first = true;
            auto &out = result.second;
            out.alert = out.alert || match->alert;
            out.showInMentions = out.showInMentions || match->showInMentions;
            if (out.sound.isEmpty() && match->sound.isValid())
            {
                out.sound = match->sound;
            }
            if (!out.color && match->color && match->color->isValid())
            {
                out.color = match->color;
            }
            if (out.full())
            {
                break;
            }
        }
    }
    return result;
}

HighlightEvaluation applyChannelHighlightPriority(
    HighlightEvaluation global, const HighlightEvaluation &local)
{
    if (!local.first)
    {
        return global;
    }
    global.first = true;
    // An explicitly silent channel match also wins over a global sound.
    global.second.color = local.second.color;
    global.second.sound = local.second.sound;
    global.second.alert = global.second.alert || local.second.alert;
    global.second.showInMentions =
        global.second.showInMentions || local.second.showInMentions;
    return global;
}

}  // namespace chatterino::satorino
