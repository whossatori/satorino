// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

class GeneralPageView;

class SatorinoPage : public SettingsPage
{
    Q_OBJECT

public:
    SatorinoPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);
    void addLiveStatus(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
