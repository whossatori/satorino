// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QLabel;
class QListWidget;

namespace chatterino {

// satorino: dedicated Themes settings page.
//
// Shares the theme list and the "/appearance/theme/name" setting with the
// theme dropdown in Settings -> General - there is no duplicated state.
// See docs/SATORINO.md.
class ThemesPage : public SettingsPage
{
public:
    ThemesPage();

    bool filterElements(const QString &query) override;

    void onShow() override;

private:
    void rebuildThemeList();
    void applyTheme(int row);
    void openThemesFolder();

    QListWidget *list_{};
    QLabel *hint_{};
};

}  // namespace chatterino
