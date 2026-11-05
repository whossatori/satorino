// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"
#include <QTimer>  // satorino: visible-page-only raid diagnostics

class QLabel;
class QShowEvent;
class QHideEvent;

namespace chatterino {

class GeneralPageView;

class SatorinoPage : public SettingsPage
{
    Q_OBJECT

public:
    SatorinoPage();

    bool filterElements(const QString &query) override;

protected:
    void showEvent(
        QShowEvent *event) override;  // satorino: no background page polling
    void hideEvent(QHideEvent *event) override;

private:
    void refreshRaidStatus();
    QTimer raidStatusTimer_;
    QLabel *raidStatus_ = nullptr;
    void initLayout(GeneralPageView &layout);
    void addLiveStatus(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
