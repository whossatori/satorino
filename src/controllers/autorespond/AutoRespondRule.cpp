// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondRule.hpp"
#include <QUuid>
#include <utility>

namespace chatterino::satorino {
AutoRespondTimer AutoRespondRule::timerRule() const
{
    return {this->id,       this->enabled, this->label, this->channel,
            this->response, this->time,    this->days};
}
AutoRespondRule newAutoRespondRule(AutoRespondType type)
{
    AutoRespondRule rule;
    rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.type = type;
    rule.enabled = false;
    return rule;
}
std::vector<AutoRespondRule> readAutoRespondRules(
    const rapidjson::Value *unified, const rapidjson::Value *triggers,
    const rapidjson::Value *timers)
{
    // An explicit empty new list means deleted rules: never resurrect legacy data.
    if (unified)
        return unified->IsArray()
                   ? pajlada::Deserialize<std::vector<AutoRespondRule>>::get(
                         *unified)
                   : std::vector<AutoRespondRule>{};
    std::vector<AutoRespondRule> result;
    if (triggers && triggers->IsArray())
        for (const auto &old :
             pajlada::Deserialize<std::vector<AutoRespondTrigger>>::get(
                 *triggers))
        {
            AutoRespondRule rule;
            static_cast<AutoRespondTrigger &>(rule) = old;
            result.push_back(std::move(rule));
        }
    if (timers && timers->IsArray())
        for (const auto &old :
             pajlada::Deserialize<std::vector<AutoRespondTimer>>::get(*timers))
        {
            AutoRespondRule rule;
            rule.type = AutoRespondType::Timer;
            rule.id = old.id;
            rule.enabled = old.enabled;
            rule.label = old.label;
            rule.channel = old.channel;
            rule.response = old.message;
            rule.time = old.time;
            rule.days = old.days;
            result.push_back(std::move(rule));
        }
    return result;
}
}  // namespace chatterino::satorino

namespace pajlada {
rapidjson::Value Serialize<chatterino::satorino::AutoRespondRule>::get(
    const chatterino::satorino::AutoRespondRule &value,
    rapidjson::Document::AllocatorType &a)
{
    auto out =
        Serialize<chatterino::satorino::AutoRespondTrigger>::get(value, a);
    chatterino::rj::set(
        out, "type",
        QString(value.type == chatterino::satorino::AutoRespondType::Timer
                    ? "timer"
                    : "trigger"),
        a);
    chatterino::rj::set(out, "time", value.time, a);
    chatterino::rj::set(out, "days", value.days, a);
    return out;
}
chatterino::satorino::AutoRespondRule
    Deserialize<chatterino::satorino::AutoRespondRule>::get(
        const rapidjson::Value &value, bool *error)
{
    chatterino::satorino::AutoRespondRule out;
    static_cast<chatterino::satorino::AutoRespondTrigger &>(out) =
        Deserialize<chatterino::satorino::AutoRespondTrigger>::get(value,
                                                                   error);
    QString type;
    chatterino::rj::getSafe(value, "type", type);
    out.type = type == "timer" ? chatterino::satorino::AutoRespondType::Timer
                               : chatterino::satorino::AutoRespondType::Trigger;
    chatterino::rj::getSafe(value, "time", out.time);
    chatterino::rj::getSafe(value, "days", out.days);
    out.days &= 127;
    return out;
}
}  // namespace pajlada
