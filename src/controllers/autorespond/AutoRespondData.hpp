// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/RapidjsonHelpers.hpp"
#include "common/enums/MessageContext.hpp"
#include "messages/MessageFlag.hpp"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include <QStringView>

#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace chatterino::satorino {

struct AutoRespondTrigger {
    QString id;
    bool enabled = false;
    QString label;
    QString channel = "*";
    QString pattern;
    bool regex = false;
    bool caseSensitive = false;
    QString response;
    bool sound = false;
    QString customSound;
    int cooldownSeconds = 30;
    int delayMs = 0;
    bool operator==(const AutoRespondTrigger &) const = default;
};

struct AutoRespondTimer {
    QString id;
    bool enabled = false;
    QString label;
    QString channel = "*";
    QString message;
    QString time = "12:00";
    int days = 127;  // bit 0 = Monday, bit 6 = Sunday
    bool operator==(const AutoRespondTimer &) const = default;
};

struct AutoRespondInput {
    QString channel;  // canonical platform:name
    QString user;
    QString text;
    bool live = false;
    bool loggedIn = false;
    bool anonymous = true;
    bool own = false;
    bool autoSent = false;
};

bool isAutoRespondLiveMessage(MessageContext context, MessageFlags flags);
bool canAutoRespond(const AutoRespondInput &input);
QRegularExpression autoRespondExpression(const AutoRespondTrigger &rule);
std::optional<QRegularExpressionMatch> matchAutoRespond(
    const AutoRespondTrigger &rule, const AutoRespondInput &input);
// Accepts a cached expression compiled when rules change.
std::optional<QRegularExpressionMatch> matchAutoRespond(
    const AutoRespondTrigger &rule, const AutoRespondInput &input,
    const QRegularExpression &expression);
// Replaces controls/line separators, preserves surrogate pairs and caps UTF-16
// units AND UTF-8 bytes (at most 500). The bound fits either platform's limit.
QString sanitizeAutoRespond(QStringView text, int maxLength = 500);
QString expandAutoRespond(QStringView text, const AutoRespondInput &input,
                          const QRegularExpressionMatch &match,
                          int maxLength = 500);

class AutoRespondLimits
{
public:
    bool reserveRule(const QString &id, int cooldownSeconds, qint64 nowMs);
    void rememberSent(const QString &channel, const QString &text,
                      qint64 nowMs);
    bool wasAutoSent(const QString &channel, const QString &text, qint64 nowMs);

private:
    struct Echo {
        QString channel;
        QString text;
        qint64 at;
    };
    std::map<QString, qint64> rules_;
    std::deque<Echo> echoes_;
};

// Pure scheduling: no ambient clock. Missing spring-forward times are skipped;
// repeated fall-back times use their first occurrence. lastConsumed is a local
// date high-water mark, including offline/skipped occurrences.
std::optional<QDateTime> nextAutoRespondFire(const AutoRespondTimer &rule,
                                             const QDateTime &now,
                                             const QDate &lastConsumed,
                                             const QTimeZone &zone);
bool autoRespondTimerDue(const AutoRespondTimer &rule,
                         const QDateTime &previous, const QDateTime &now,
                         const QDate &lastConsumed, const QTimeZone &zone);

}  // namespace chatterino::satorino

namespace pajlada {
template <>
struct Serialize<chatterino::satorino::AutoRespondTrigger> {
    static rapidjson::Value get(
        const chatterino::satorino::AutoRespondTrigger &v,
        rapidjson::Document::AllocatorType &a);
};
template <>
struct Deserialize<chatterino::satorino::AutoRespondTrigger> {
    static chatterino::satorino::AutoRespondTrigger get(
        const rapidjson::Value &v, bool *error = nullptr);
};
template <>
struct Serialize<chatterino::satorino::AutoRespondTimer> {
    static rapidjson::Value get(const chatterino::satorino::AutoRespondTimer &v,
                                rapidjson::Document::AllocatorType &a);
};
template <>
struct Deserialize<chatterino::satorino::AutoRespondTimer> {
    static chatterino::satorino::AutoRespondTimer get(const rapidjson::Value &v,
                                                      bool *error = nullptr);
};
}  // namespace pajlada
