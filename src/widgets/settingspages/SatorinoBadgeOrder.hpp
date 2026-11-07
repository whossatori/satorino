// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/signals/signalholder.hpp>
#include <QTimer>
#include <QWidget>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QShowEvent;
class QHideEvent;

namespace chatterino {
class SatorinoBadgeOrder : public QWidget
{
public:
    explicit SatorinoBadgeOrder(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void rebuildOrder();
    void saveOrder();
    void moveSelection(int delta);
    void updateButtons();
    void refreshPreview();
    QListWidget *list_;
    QComboBox *scope_;
    QPushButton *up_;
    QPushButton *down_;
    QLabel *previewLabel_;
    QHBoxLayout *preview_;
    QTimer refreshTimer_;
    QTimer saveTimer_;
    bool updating_ = false;
    pajlada::Signals::SignalHolder connections_;
};
}  // namespace chatterino
