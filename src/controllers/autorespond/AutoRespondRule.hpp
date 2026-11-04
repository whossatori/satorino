// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include "controllers/autorespond/AutoRespondData.hpp"

namespace chatterino::satorino {
enum class AutoRespondType { Trigger, Timer };
struct AutoRespondRule : AutoRespondTrigger {
    AutoRespondType type = AutoRespondType::Trigger;
    QString time = "12:00";
    int days = 127;
    AutoRespondTimer timerRule() const;
    bool operator==(const AutoRespondRule &) const = default;
};
AutoRespondRule newAutoRespondRule(
    AutoRespondType type = AutoRespondType::Trigger);
std::vector<AutoRespondRule> readAutoRespondRules(
    const rapidjson::Value *unified, const rapidjson::Value *triggers,
    const rapidjson::Value *timers);
}  // namespace chatterino::satorino

namespace pajlada {
template <>
struct Serialize<chatterino::satorino::AutoRespondRule> {
    static rapidjson::Value get(
        const chatterino::satorino::AutoRespondRule &value,
        rapidjson::Document::AllocatorType &allocator);
};
template <>
struct Deserialize<chatterino::satorino::AutoRespondRule> {
    static chatterino::satorino::AutoRespondRule get(
        const rapidjson::Value &value, bool *error = nullptr);
};
}  // namespace pajlada
