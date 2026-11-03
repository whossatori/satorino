// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/Theme.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <optional>
#include <vector>

class QLabel;

namespace chatterino {

class FlowLayout;
class ThemeCard;

// satorino: dedicated Themes settings page.
//
// The themes are shown as a grid of preview cards (ThemeCard) inside a
// panel. The page owns no theme state of its own: activating a card writes
// the same "/appearance/theme/name" setting as /settheme, and the
// highlighted card follows that setting in both directions. See
// docs/SATORINO.md.
class ThemesPage : public SettingsPage
{
public:
    ThemesPage();

    bool filterElements(const QString &query) override;

    void onShow() override;

private:
    void addCard(const QString &title, const QString &key,
                 const ThemePreviewColors &primary,
                 const std::optional<ThemePreviewColors> &secondary =
                     std::nullopt,
                 const QString &tooltip = {});

    void rebuildThemeList();

    void updateSelectedCard();

    void openThemesFolder();

    FlowLayout *flow_{};
    QLabel *hint_{};
    std::vector<ThemeCard *> cards_{};
};

}  // namespace chatterino
