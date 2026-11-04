// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/SatorinoTitleLabel.hpp"

#include "Application.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"

#include <QEvent>
#include <QFontMetricsF>
#include <QHideEvent>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QTextOption>
#include <QShowEvent>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace chatterino {

TitlePaintAnimation loadSatorinoTitlePaint()
{
    QImageReader reader(":/satorino-paint.webp", "webp");
    if (!reader.canRead() || !reader.supportsAnimation() ||
        reader.imageCount() <= 1)
    {
        return {};
    }

    TitlePaintAnimation animation;
    animation.loops = reader.loopCount();
    const int count = reader.imageCount();
    animation.frames.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        auto image = reader.read();
        if (image.isNull())
        {
            return {};
        }
        // Keep authored frame delays; only guard invalid/zero delays.
        animation.frames.push_back(
            {std::move(image), std::max(1, reader.nextImageDelay())});
    }
    return animation;
}

SatorinoTitleLabel::SatorinoTitleLabel()
{
    this->updateSize();
    getSettings()->satorinoAnimatedTitlePaint.connect(
        [this](bool enabled) {
            this->setPaintEnabled(enabled);
        },
        this->connections_);
    this->timer_.setSingleShot(true);
    this->timer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->timer_, &QTimer::timeout, this,
                     &SatorinoTitleLabel::advanceFrame);
    QObject::connect(&this->loader_,
                     &QFutureWatcher<TitlePaintAnimation>::finished, this,
                     [this] {
                         this->animation_ = this->loader_.result();
                         this->remainingLoops_ = this->animation_.loops;
                         this->update();
                         this->syncTimer();
                     });
}

void SatorinoTitleLabel::setPaintEnabled(bool enabled)
{
    this->enabled_ = enabled;
    this->loadIfNeeded();
    this->syncTimer();
    this->update();
}

void SatorinoTitleLabel::loadIfNeeded()
{
    if (this->enabled_ && this->isVisible() && !this->loadRequested_)
    {
        this->loadRequested_ = true;
        // No decoding, resource IO, or image-plugin discovery on the GUI thread.
        this->loader_.setFuture(QtConcurrent::run(loadSatorinoTitlePaint));
    }
}

void SatorinoTitleLabel::showEvent(QShowEvent *event)
{
    Label::showEvent(event);
    if (this->observedWindow_ != this->window())
    {
        if (this->observedWindow_)
        {
            this->observedWindow_->removeEventFilter(this);
        }
        this->observedWindow_ = this->window();
        this->observedWindow_->installEventFilter(this);
    }
    this->loadIfNeeded();
    this->syncTimer();
}

void SatorinoTitleLabel::hideEvent(QHideEvent *event)
{
    this->pauseTimer();
    Label::hideEvent(event);
}

bool SatorinoTitleLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this->observedWindow_)
    {
        if (event->type() == QEvent::Hide ||
            event->type() == QEvent::HideToParent)
        {
            this->pauseTimer();
        }
        else if (event->type() == QEvent::Show ||
                 event->type() == QEvent::WindowStateChange)
        {
            this->syncTimer();
        }
    }
    return Label::eventFilter(watched, event);
}

bool SatorinoTitleLabel::canAnimate() const
{
    return this->enabled_ && this->isVisible() && this->window()->isVisible() &&
           !this->window()->isMinimized() && this->animation_.frames.size() > 1;
}

void SatorinoTitleLabel::pauseTimer()
{
    if (this->timer_.isActive())
    {
        this->remainingDelay_ = std::max(1, this->timer_.remainingTime());
        this->timer_.stop();
    }
}

void SatorinoTitleLabel::syncTimer()
{
    if (!this->canAnimate())
    {
        this->pauseTimer();
    }
    else if (!this->finished_ && !this->timer_.isActive())
    {
        const int delay = this->remainingDelay_ > 0
                              ? this->remainingDelay_
                              : this->animation_.frames[this->frame_].delay;
        this->remainingDelay_ = -1;
        this->timer_.start(delay);
    }
}

void SatorinoTitleLabel::advanceFrame()
{
    if (!this->canAnimate())
    {
        return;
    }
    if (this->frame_ + 1 == this->animation_.frames.size())
    {
        if (this->remainingLoops_ == 0)
        {
            this->finished_ = true;
            return;
        }
        if (this->remainingLoops_ > 0)
        {
            --this->remainingLoops_;
        }
        this->frame_ = 0;
    }
    else
    {
        ++this->frame_;
    }
    this->update();
    this->syncTimer();
}

QFont SatorinoTitleLabel::titleFont() const
{
    auto font =
        getApp()->getFonts()->getFont(this->getFontStyle(), this->scale());
    font.setWeight(QFont::Bold);
    return font;
}

void SatorinoTitleLabel::updateSize()
{
    this->currentPadding_ = this->basePadding_.toMarginsF() * this->scale();
    const QFontMetricsF metrics(this->titleFont());
    const auto width = metrics.horizontalAdvance(this->getText()) +
                       this->currentPadding_.left() +
                       this->currentPadding_.right();
    const auto height = metrics.height() + this->currentPadding_.top() +
                        this->currentPadding_.bottom();
    this->sizeHint_ = QSize(static_cast<int>(std::ceil(width)),
                            static_cast<int>(std::ceil(height)));
    this->minimumSizeHint_ = this->sizeHint_;
    this->updateGeometry();
    this->update();
}

void SatorinoTitleLabel::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    const auto font = this->titleFont();
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    const auto area = this->textRect();
    const QRectF bounds(area.left(), area.center().y() - metrics.height() / 2,
                        metrics.horizontalAdvance(this->getText()),
                        metrics.height());
    if (bounds.isEmpty())
    {
        return;
    }

    QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
    option.setWrapMode(QTextOption::NoWrap);
    if (!this->enabled_ || this->animation_.frames.size() <= 1)
    {
        // The fallback and animated glyphs share the font used by sizeHint.
        painter.setPen(this->palette().windowText().color());
        painter.drawText(area, this->getText(), option);
        return;
    }

    // Fill the bold glyphs at the widget's DPR, using the same text rectangle.
    const qreal dpr = this->devicePixelRatioF();
    QPixmap texture((bounds.size() * dpr).toSize());
    texture.setDevicePixelRatio(dpr);
    texture.fill(Qt::transparent);
    {
        QPainter imagePainter(&texture);
        imagePainter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto &image = this->animation_.frames[this->frame_].image;
        const auto cover =
            QSizeF(image.size())
                .scaled(bounds.size(), Qt::KeepAspectRatioByExpanding);
        const QRectF target((bounds.width() - cover.width()) / 2,
                            (bounds.height() - cover.height()) / 2,
                            cover.width(), cover.height());
        imagePainter.drawImage(target, image);
    }
    QBrush brush(texture);
    QTransform origin;
    origin.translate(bounds.left(), bounds.top());
    brush.setTransform(origin);
    painter.setPen(QPen(brush, 0));
    painter.drawText(area, this->getText(), option);
}

}  // namespace chatterino
