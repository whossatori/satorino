// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include <QString>
namespace chatterino {
struct CommandContext;
}
namespace chatterino::commands {
QString autoRespond(const CommandContext &ctx);
}  // namespace chatterino::commands
