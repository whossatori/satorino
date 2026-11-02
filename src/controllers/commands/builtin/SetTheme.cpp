// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/SetTheme.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "singletons/Theme.hpp"

#include <QStringList>

namespace chatterino::commands {

QString setTheme(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    auto *theme = getTheme();
    const auto available = theme->availableThemes();

    const auto argument = ctx.words.mid(1).join(u' ').trimmed();

    if (!argument.isEmpty())
    {
        const auto customPrefix = QStringLiteral("Custom: ");
        const auto matches = [&argument](const QString &candidate) {
            return candidate.compare(argument, Qt::CaseInsensitive) == 0;
        };

        for (const auto &[name, key] : available)
        {
            const auto keyString = key.toString();
            // Accept the display name ("Custom: Foo"), the bare custom
            // theme name ("Foo") and the key ("Foo.json").
            if (matches(name) || matches(keyString) ||
                (name.startsWith(customPrefix) &&
                 matches(name.mid(customPrefix.size()).trimmed())))
            {
                // Same setting and apply path as the Themes page and the
                // theme dropdown in Settings -> General.
                theme->themeName = keyString;
                ctx.channel->addSystemMessage(
                    QStringLiteral("Theme set to \"%1\".").arg(name));
                return "";
            }
        }
    }

    QStringList names;
    for (const auto &[name, key] : available)
    {
        names << name;
    }

    if (argument.isEmpty())
    {
        ctx.channel->addSystemMessage(
            QStringLiteral("Usage: /settheme <theme name>. Available "
                           "themes: %1")
                .arg(names.join(QStringLiteral(", "))));
    }
    else
    {
        ctx.channel->addSystemMessage(
            QStringLiteral("Unknown theme \"%1\". Available themes: %2")
                .arg(argument, names.join(QStringLiteral(", "))));
    }

    return "";
}

}  // namespace chatterino::commands
