// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace chatterino {

struct CommandContext;

namespace commands {

// satorino: /settheme <theme name> - applies and persists a theme.
// Matching is case-insensitive; multi-word names are joined, and both the
// display name ("Custom: Foo"), the bare custom theme name ("Foo") and the
// file key ("Foo.json") are accepted. With no argument or an unknown name,
// the available themes are listed as a local system message.
// See docs/SATORINO.md.
QString setTheme(const CommandContext &ctx);

}  // namespace commands
}  // namespace chatterino
