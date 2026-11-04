// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/AutoRespond.hpp"

#include "common/Channel.hpp"
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "controllers/commands/CommandContext.hpp"

namespace chatterino::commands {

QString autoRespond(const CommandContext &ctx)
{
    if (!ctx.channel)
        return {};
    auto *available = satorino::tryAutoRespondSettings();
    if (!available)
    {
        ctx.channel->addSystemMessage(
            "Auto respond is unavailable while the app starts or stops.");
        return {};
    }
    auto &settings = *available;
    const auto action = ctx.words.value(1).toLower();
    if (ctx.words.size() != 2 ||
        (action != "on" && action != "off" && action != "status"))
    {
        ctx.channel->addSystemMessage("Usage: /autorespond on|off|status");
        return {};
    }
    if (action == "on")
        settings.enabled = true;
    if (action == "off")
        settings.enabled = false;
    ctx.channel->addSystemMessage(
        QString("Auto respond is %1 (maximum %2 messages per minute).")
            .arg(settings.enabled.getValue() ? "on" : "off")
            .arg(settings.perMinute.getValue()));
    return {};
}
}  // namespace chatterino::commands
