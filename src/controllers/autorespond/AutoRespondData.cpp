// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondData.hpp"

#include "controllers/highlights/types/Common.hpp"

#include <algorithm>

namespace chatterino::satorino {

bool isAutoRespondLiveMessage(MessageContext context, MessageFlags flags)
{
    return context == MessageContext::Original &&
           !flags.hasAny(
               {MessageFlag::System, MessageFlag::RecentMessage,
                MessageFlag::Whisper, MessageFlag::Disabled,
                MessageFlag::AutoMod, MessageFlag::AutoModOffendingMessage,
                MessageFlag::SharedMessage, MessageFlag::ModerationAction});
}

bool canAutoRespond(const AutoRespondInput &input)
{
    return input.live && input.loggedIn && !input.anonymous && !input.own &&
           !input.autoSent && !input.user.isEmpty() &&
           (input.channel.startsWith(u"twitch:") ||
            input.channel.startsWith(u"kick:"));
}

QRegularExpression autoRespondExpression(const AutoRespondTrigger &rule)
{
    const auto pattern = rule.regex
                             ? rule.pattern
                             : highlights::REGEX_START_BOUNDARY.toString() +
                                   QRegularExpression::escape(rule.pattern) +
                                   highlights::REGEX_END_BOUNDARY.toString();
    auto options = QRegularExpression::UseUnicodePropertiesOption;
    QRegularExpression::PatternOptions flags(options);
    if (!rule.caseSensitive)
    {
        flags |= QRegularExpression::CaseInsensitiveOption;
    }
    return QRegularExpression(pattern, flags);
}

std::optional<QRegularExpressionMatch> matchAutoRespond(
    const AutoRespondTrigger &rule, const AutoRespondInput &input)
{
    return matchAutoRespond(rule, input, autoRespondExpression(rule));
}

std::optional<QRegularExpressionMatch> matchAutoRespond(
    const AutoRespondTrigger &rule, const AutoRespondInput &input,
    const QRegularExpression &expression)
{
    if (!rule.enabled || rule.pattern.isEmpty() || !canAutoRespond(input) ||
        (rule.channel != "*" && rule.channel != input.channel))
    {
        return std::nullopt;
    }
    if (!expression.isValid())
    {
        return std::nullopt;
    }
    const auto match = expression.match(input.text);
    return match.hasMatch() ? std::optional(match) : std::nullopt;
}

QString sanitizeAutoRespond(QStringView text, int maxLength)
{
    maxLength = std::min(maxLength, 500);
    if (maxLength <= 0)
    {
        return {};
    }
    QString result;
    int bytes = 0;
    for (qsizetype i = 0; i < text.size(); ++i)
    {
        QChar ch = text[i];
        QString part;
        if (ch.isHighSurrogate() && i + 1 < text.size() &&
            text[i + 1].isLowSurrogate())
        {
            part.append(ch);
            part.append(text[++i]);
        }
        else if (ch.isSurrogate())
        {
            part = QString(QChar::ReplacementCharacter);
        }
        else if (ch.category() == QChar::Other_Control ||
                 ch.category() == QChar::Separator_Line ||
                 ch.category() == QChar::Separator_Paragraph)
        {
            part = " ";
        }
        else
        {
            part.append(ch);
        }
        const int n = static_cast<int>(part.toUtf8().size());
        if (result.size() + part.size() > maxLength || bytes + n > maxLength)
        {
            break;
        }
        bytes += n;
        result += part;
    }
    return result.simplified();
}

QString expandAutoRespond(QStringView text, const AutoRespondInput &input,
                          const QRegularExpressionMatch &match, int maxLength)
{
    maxLength = std::clamp(maxLength, 0, 500);
    // Single pass: placeholders introduced by a username or capture are literal.
    QString result;
    for (qsizetype i = 0; i < text.size() && result.size() <= maxLength * 2;
         ++i)
    {
        if (text[i] == u'{')
        {
            const auto end = text.indexOf(u'}', i + 1);
            if (end >= 0)
            {
                const auto key = text.mid(i + 1, end - i - 1);
                QString value;
                bool known = true;
                if (key == u"user")
                    value = input.user;
                else if (key == u"channel")
                    value = input.channel.section(u':', 1);
                else if (key == u"match")
                    value = match.captured(0);
                else if (key.size() == 1 && key[0] >= u'1' && key[0] <= u'9')
                {
                    const auto capture = key[0].unicode() - u'0';
                    if (capture <= match.lastCapturedIndex())
                        value = match.captured(capture);
                }
                else
                    known = false;
                if (known)
                {
                    result += sanitizeAutoRespond(value, maxLength);
                    i = end;
                    continue;
                }
            }
        }
        result += text[i];
    }
    return sanitizeAutoRespond(result, maxLength);
}

bool AutoRespondLimits::reserveRule(const QString &id, int cooldownSeconds,
                                    qint64 nowMs)
{
    const auto found = this->rules_.find(id);
    if (found != this->rules_.end() &&
        nowMs - found->second < qint64(std::max(1, cooldownSeconds)) * 1000)
    {
        return false;
    }
    this->rules_[id] = nowMs;
    return true;
}

void AutoRespondLimits::rememberSent(const QString &channel,
                                     const QString &text, qint64 nowMs)
{
    this->echoes_.push_back({channel, sanitizeAutoRespond(text), nowMs});
    while (this->echoes_.size() > 512)
        this->echoes_.pop_front();
}

bool AutoRespondLimits::wasAutoSent(const QString &channel, const QString &text,
                                    qint64 nowMs)
{
    while (!this->echoes_.empty() && nowMs - this->echoes_.front().at >= 60000)
    {
        this->echoes_.pop_front();
    }
    const auto normalized = sanitizeAutoRespond(text);
    return std::ranges::any_of(this->echoes_, [&](const auto &echo) {
        return echo.channel == channel && echo.text == normalized;
    });
}

std::optional<QDateTime> nextAutoRespondFire(const AutoRespondTimer &rule,
                                             const QDateTime &now,
                                             const QDate &lastConsumed,
                                             const QTimeZone &zone)
{
    const auto time = QTime::fromString(rule.time, "HH:mm");
    if (!rule.enabled || !time.isValid() || rule.days == 0 || !now.isValid())
    {
        return std::nullopt;
    }
    const auto today = now.toTimeZone(zone).date();
    const auto start = lastConsumed.isValid() && lastConsumed >= today
                           ? lastConsumed.addDays(1)
                           : today;
    for (int offset = 0; offset < 8; ++offset)
    {
        const auto day = start.addDays(offset);
        if (!(rule.days & (1 << (day.dayOfWeek() - 1))))
            continue;
        const QDateTime candidate(
            day, time, zone, QDateTime::TransitionResolution::PreferBefore);
        // A nonexistent local time must not slide to a different wall time.
        if (candidate.isValid() && candidate.date() == day &&
            candidate.time() == time && candidate > now)
        {
            return candidate;
        }
    }
    return std::nullopt;
}

bool autoRespondTimerDue(const AutoRespondTimer &rule,
                         const QDateTime &previous, const QDateTime &now,
                         const QDate &lastConsumed, const QTimeZone &zone)
{
    // Only a normal short tick may cross a deadline. Startup, suspend/resume,
    // wall-clock jumps and long event-loop stalls skip it instead of catching up.
    const auto elapsed = previous.msecsTo(now);
    if (elapsed <= 0 || elapsed > 2500)
        return false;
    const auto next = nextAutoRespondFire(rule, previous, lastConsumed, zone);
    return next && *next <= now;
}

}  // namespace chatterino::satorino

namespace pajlada {
using chatterino::rj::getSafe;
using chatterino::rj::set;

rapidjson::Value Serialize<chatterino::satorino::AutoRespondTrigger>::get(
    const chatterino::satorino::AutoRespondTrigger &v,
    rapidjson::Document::AllocatorType &a)
{
    rapidjson::Value out(rapidjson::kObjectType);
    set(out, "id", v.id, a);
    set(out, "enabled", v.enabled, a);
    set(out, "label", v.label, a);
    set(out, "channel", v.channel, a);
    set(out, "pattern", v.pattern, a);
    set(out, "regex", v.regex, a);
    set(out, "caseSensitive", v.caseSensitive, a);
    set(out, "response", v.response, a);
    set(out, "sound", v.sound, a);
    set(out, "customSound", v.customSound, a);
    set(out, "cooldownSeconds", v.cooldownSeconds, a);
    set(out, "delayMs", v.delayMs, a);
    return out;
}

chatterino::satorino::AutoRespondTrigger
    Deserialize<chatterino::satorino::AutoRespondTrigger>::get(
        const rapidjson::Value &v, bool *error)
{
    chatterino::satorino::AutoRespondTrigger out;
    if (!v.IsObject())
    {
        PAJLADA_REPORT_ERROR(error);
        return out;
    }
    getSafe(v, "id", out.id);
    getSafe(v, "enabled", out.enabled);
    getSafe(v, "label", out.label);
    getSafe(v, "channel", out.channel);
    getSafe(v, "pattern", out.pattern);
    getSafe(v, "regex", out.regex);
    getSafe(v, "caseSensitive", out.caseSensitive);
    getSafe(v, "response", out.response);
    getSafe(v, "sound", out.sound);
    getSafe(v, "customSound", out.customSound);
    getSafe(v, "cooldownSeconds", out.cooldownSeconds);
    getSafe(v, "delayMs", out.delayMs);
    out.cooldownSeconds = std::clamp(out.cooldownSeconds, 1, 86400);
    out.delayMs = std::clamp(out.delayMs, 0, 3600000);
    return out;
}

rapidjson::Value Serialize<chatterino::satorino::AutoRespondTimer>::get(
    const chatterino::satorino::AutoRespondTimer &v,
    rapidjson::Document::AllocatorType &a)
{
    rapidjson::Value out(rapidjson::kObjectType);
    set(out, "id", v.id, a);
    set(out, "enabled", v.enabled, a);
    set(out, "label", v.label, a);
    set(out, "channel", v.channel, a);
    set(out, "message", v.message, a);
    set(out, "time", v.time, a);
    set(out, "days", v.days, a);
    return out;
}

chatterino::satorino::AutoRespondTimer
    Deserialize<chatterino::satorino::AutoRespondTimer>::get(
        const rapidjson::Value &v, bool *error)
{
    chatterino::satorino::AutoRespondTimer out;
    if (!v.IsObject())
    {
        PAJLADA_REPORT_ERROR(error);
        return out;
    }
    getSafe(v, "id", out.id);
    getSafe(v, "enabled", out.enabled);
    getSafe(v, "label", out.label);
    getSafe(v, "channel", out.channel);
    getSafe(v, "message", out.message);
    getSafe(v, "time", out.time);
    getSafe(v, "days", out.days);
    out.days &= 127;
    return out;
}
}  // namespace pajlada
