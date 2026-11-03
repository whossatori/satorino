// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/Theme.hpp"

#include <QColor>
#include <QString>
#include <QWidget>

#include <optional>

class QEnterEvent;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace chatterino {

// satorino: one card in the Themes settings page grid (docs/SATORINO.md).
//
// A fixed-size preview card: five square swatches (the theme background
// plus four accents from Theme::previewColors()) with the theme name
// centred underneath. The card chrome is pinned to the settings dialog's
// FIXED dark style - only the swatches vary by theme.
class ThemeCard : public QWidget
{
    Q_OBJECT

public:
    // `primary` holds the theme's preview colors. When `secondary` is set,
    // every swatch is split diagonally (primary top-left, secondary
    // bottom-right) - used by the adaptive System card to preview both the
    // dark and the light system theme.
    // `key` is the value written to the themeName setting when the card is
    // activated (the page performs the write, the card only reports it).
    ThemeCard(const QString &title, const QString &key,
              const ThemePreviewColors &primary,
              std::optional<ThemePreviewColors> secondary = std::nullopt,
              QWidget *parent = nullptr);

    [[nodiscard]] const QString &title() const;

    [[nodiscard]] const QString &key() const;

    [[nodiscard]] bool isSelected() const;
    void setSelected(bool value);

Q_SIGNALS:
    // Emitted on click and on Enter/Space. The page applies the theme
    // exactly like /settheme does (write the themeName setting).
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QString title_;

    QString key_;

    ThemePreviewColors primary_;

    std::optional<ThemePreviewColors> secondary_;

    bool selected_ = false;
};

}  // namespace chatterino
