// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/Label.hpp"

#include <QFutureWatcher>
#include <QImage>
#include <QPointer>
#include <QTimer>

#include <cstddef>
#include <vector>

namespace chatterino {

struct TitlePaintFrame {
    QImage image;
    int delay = 0;
};

struct TitlePaintAnimation {
    std::vector<TitlePaintFrame> frames;
    // QImageReader repetitions after the first play; -1 means forever.
    int loops = 0;
};

// Uses only the embedded resource and Qt's webp handler; safe on a worker.
TitlePaintAnimation loadSatorinoTitlePaint();

class SatorinoTitleLabel : public Label
{
public:
    SatorinoTitleLabel();
    void setPaintEnabled(bool enabled);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void updateSize() override;

private:
    QFont titleFont() const;
    void loadIfNeeded();
    bool canAnimate() const;
    void syncTimer();
    void pauseTimer();
    void advanceFrame();

    QPointer<QWidget> observedWindow_;
    QFutureWatcher<TitlePaintAnimation> loader_;
    TitlePaintAnimation animation_;
    QTimer timer_;
    std::size_t frame_ = 0;
    int remainingLoops_ = 0;
    int remainingDelay_ = -1;
    bool enabled_ = true;
    bool loadRequested_ = false;
    bool finished_ = false;
};

}  // namespace chatterino
