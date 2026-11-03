// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ThemeCard.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <Qt>

namespace {

using namespace chatterino;

// Card chrome. The settings dialog is pinned dark
// (BaseWindow::UseSettingsStylesheet + resources/qss/settings.qss + the
// app-wide dark palette from RunGui), so the chrome reuses the app's own
// colors to match the rest of the dialog: #333 is QPalette::Base - the
// surface of every combo box in the dialog - #444/#545454 are the app's
// border greys (settings.qss paints the tooltip border #545454) and
// #4FC3F7 is the accent settings.qss paints section titles with. Only the
// swatches vary by theme - the grid looks identical in every app theme,
// like the rest of the dialog.
const QColor CARD_BACKGROUND{0x33, 0x33, 0x33};
const QColor CARD_BORDER{0x44, 0x44, 0x44};
const QColor CARD_BORDER_HOVER{0x54, 0x54, 0x54};
const QColor CARD_SELECTED_BORDER{0x4f, 0xc3, 0xf7};
const QColor CARD_TEXT{0xff, 0xff, 0xff};

constexpr int CARD_WIDTH = 130;
constexpr int CARD_HEIGHT = 85;
constexpr double CARD_RADIUS = 6.0;

constexpr int SWATCH_SIZE = 19;
constexpr int SWATCH_GAP = 4;
constexpr int SWATCH_COUNT = 5;
constexpr int SWATCH_MARGIN_TOP = 10;
constexpr double SWATCH_RADIUS = 2.0;

constexpr int TITLE_AREA_TOP = 36;
constexpr int TITLE_LINE_STEP = 16;
constexpr int TITLE_MAX_LINES = 2;
constexpr int TITLE_FONT_PIXEL = 12;
constexpr int TITLE_SIDE_MARGIN = 6;

const QColor &previewAt(const ThemePreviewColors &colors, int index)
{
    switch (index)
    {
        case 0:
            return colors.background;
        case 1:
            return colors.accent;
        case 2:
            return colors.selectedTab;
        case 3:
            return colors.link;
        default:
            return colors.highlightFlash;
    }
}

// Subtle outline so near-black swatches do not melt into the card.
QColor swatchBorder(const QColor &color)
{
    return color.lightnessF() < 0.25 ? QColor(255, 255, 255, 40)
                                     : QColor(0, 0, 0, 40);
}

// Greedy word wrap into at most `maxLines` lines, eliding the last line.
QStringList wrapTitle(const QString &title, const QFont &font, int maxWidth,
                      int maxLines)
{
    const QFontMetrics metrics(font);
    const auto fits = [&](const QString &text) {
        return metrics.horizontalAdvance(text) <= maxWidth;
    };

    QStringList lines;
    QString current;

    for (const auto &word : title.split(u' ', Qt::SkipEmptyParts))
    {
        const auto candidate = current.isEmpty() ? word : current + u' ' + word;

        if (fits(candidate) || current.isEmpty())
        {
            current = candidate;
            continue;
        }

        if (lines.size() + 1 == maxLines)
        {
            lines.append(metrics.elidedText(current + u' ' + word,
                                            Qt::ElideRight, maxWidth));
            return lines;
        }

        lines.append(current);
        current = word;
    }

    if (!current.isEmpty())
    {
        // a single word wider than the card (long file names) gets elided
        lines.append(fits(current) ? current
                                   : metrics.elidedText(current, Qt::ElideRight,
                                                        maxWidth));
    }

    return lines;
}

}  // namespace

namespace chatterino {

ThemeCard::ThemeCard(const QString &title, const QString &key,
                     const ThemePreviewColors &primary,
                     std::optional<ThemePreviewColors> secondary,
                     QWidget *parent)
    : QWidget(parent)
    , title_(title)
    , key_(key)
    , primary_(primary)
    , secondary_(std::move(secondary))
{
    this->setFixedSize(CARD_WIDTH, CARD_HEIGHT);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setCursor(Qt::PointingHandCursor);
    this->setAccessibleName(QStringLiteral("Select theme %1").arg(title));
}

const QString &ThemeCard::title() const
{
    return this->title_;
}

const QString &ThemeCard::key() const
{
    return this->key_;
}

bool ThemeCard::isSelected() const
{
    return this->selected_;
}

void ThemeCard::setSelected(bool value)
{
    if (this->selected_ == value)
    {
        return;
    }

    this->selected_ = value;
    this->update();
}

void ThemeCard::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // body + border (the border width grows inwards so the card size and
    // the flow layout never shift between states)
    const bool selected = this->selected_;
    const double borderWidth = selected ? 2.0 : 1.0;
    const QColor borderColor = selected             ? CARD_SELECTED_BORDER
                               : this->underMouse() ? CARD_BORDER_HOVER
                                                    : CARD_BORDER;

    QPainterPath body;
    body.addRoundedRect(QRectF(this->rect())
                            .adjusted(borderWidth / 2, borderWidth / 2,
                                      -borderWidth / 2, -borderWidth / 2),
                        CARD_RADIUS, CARD_RADIUS);
    painter.fillPath(body, CARD_BACKGROUND);

    QPen borderPen(borderColor);
    borderPen.setWidthF(borderWidth);
    painter.setPen(borderPen);
    painter.drawPath(body);

    // keyboard focus ring (dashed, inside the border)
    if (this->hasFocus())
    {
        QPen focusPen(CARD_SELECTED_BORDER);
        focusPen.setStyle(Qt::DashLine);
        painter.setPen(focusPen);
        painter.drawRoundedRect(QRectF(this->rect()).adjusted(3, 3, -3, -3), 4,
                                4);
    }

    // five square swatches: the theme background plus four accents,
    // centred as a row
    const double rowWidth =
        SWATCH_COUNT * SWATCH_SIZE + (SWATCH_COUNT - 1) * SWATCH_GAP;
    const double x0 = (CARD_WIDTH - rowWidth) / 2.0;

    for (int i = 0; i < SWATCH_COUNT; i++)
    {
        const QRectF rect(x0 + i * (SWATCH_SIZE + SWATCH_GAP),
                          SWATCH_MARGIN_TOP, SWATCH_SIZE, SWATCH_SIZE);

        QPainterPath swatch;
        swatch.addRoundedRect(rect, SWATCH_RADIUS, SWATCH_RADIUS);

        if (this->secondary_)
        {
            // adaptive System card: split diagonally, the dark system
            // theme top-left and the light one bottom-right
            QPainterPath upperLeft;
            upperLeft.moveTo(rect.topLeft());
            upperLeft.lineTo(rect.topRight());
            upperLeft.lineTo(rect.bottomLeft());
            upperLeft.closeSubpath();

            QPainterPath lowerRight;
            lowerRight.moveTo(rect.topRight());
            lowerRight.lineTo(rect.bottomRight());
            lowerRight.lineTo(rect.bottomLeft());
            lowerRight.closeSubpath();

            painter.save();
            painter.setClipPath(swatch);
            // semi-transparent theme colors (e.g. the highlight flash) are
            // composited over the theme's own background so the swatch
            // shows the color the way the app renders it
            painter.fillPath(upperLeft, previewAt(this->primary_, 0));
            painter.fillPath(upperLeft, previewAt(this->primary_, i));
            painter.fillPath(lowerRight, previewAt(*this->secondary_, 0));
            painter.fillPath(lowerRight, previewAt(*this->secondary_, i));
            painter.restore();
        }
        else
        {
            painter.fillPath(swatch, previewAt(this->primary_, 0));
            painter.fillPath(swatch, previewAt(this->primary_, i));
        }

        painter.setPen(QPen(swatchBorder(previewAt(this->primary_, i))));
        painter.drawPath(swatch);
    }

    // theme name, centred, at most two lines
    QFont titleFont = this->font();
    titleFont.setPixelSize(TITLE_FONT_PIXEL);
    painter.setFont(titleFont);
    painter.setPen(CARD_TEXT);

    const int titleWidth = this->width() - 2 * TITLE_SIDE_MARGIN;
    const auto lines =
        wrapTitle(this->title_, titleFont, titleWidth, TITLE_MAX_LINES);
    // keep single-line names optically centred in the two-line title area
    const int top =
        TITLE_AREA_TOP + (lines.size() == 1 ? TITLE_LINE_STEP / 2 : 0);
    for (int i = 0; i < lines.size(); i++)
    {
        painter.drawText(QRectF(TITLE_SIDE_MARGIN, top + i * TITLE_LINE_STEP,
                                titleWidth, TITLE_LINE_STEP),
                         Qt::AlignHCenter | Qt::AlignVCenter, lines.at(i));
    }
}

void ThemeCard::enterEvent(QEnterEvent * /*event*/)
{
    this->update();
}

void ThemeCard::leaveEvent(QEvent * /*event*/)
{
    this->update();
}

void ThemeCard::focusInEvent(QFocusEvent * /*event*/)
{
    this->update();
}

void ThemeCard::focusOutEvent(QFocusEvent * /*event*/)
{
    this->update();
}

void ThemeCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        Q_EMIT this->activated();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ThemeCard::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            Q_EMIT this->activated();
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

}  // namespace chatterino
