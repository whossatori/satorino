// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include "common/ChatterinoSetting.hpp"
#include "common/SignalVector.hpp"
#include "controllers/autorespond/AutoRespondRule.hpp"
#include <pajlada/signals/signalholder.hpp>

namespace chatterino::satorino {
class AutoRespondSettings
{
public:
    AutoRespondSettings();
    BoolSetting enabled{"/satorino/autorespond/enabled", false};
    IntSetting perMinute{"/satorino/autorespond/perMinute", 6};
    BoolSetting showNote{"/satorino/autorespond/showNote", true};
    ChatterinoSetting<std::vector<AutoRespondRule>> rules{
        "/satorino/autorespond/rules", {}};
    ChatterinoSetting<std::map<std::string, QString>> consumedDates{
        "/satorino/autorespond/consumedDates", {}};
    SignalVector<AutoRespondRule> rows;
    pajlada::Signals::NoArgSignal changed;

private:
    bool synchronizing_ = false;
    pajlada::Signals::SignalHolder connections_;
};
AutoRespondSettings *tryAutoRespondSettings();
}  // namespace chatterino::satorino
