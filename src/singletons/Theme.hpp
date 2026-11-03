// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/ChatterinoSetting.hpp"
#include "singletons/Paths.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/settings/setting.hpp>
#include <QColor>
#include <QJsonObject>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <memory>
#include <optional>
#include <vector>

namespace chatterino {

class WindowManager;

struct ThemeDescriptor {
    QString key;

    QString path;

    QString name;

    bool custom{};
};

// satorino: swatch colors for one theme card on the Themes settings page
// (docs/SATORINO.md). The first entry is the theme background, the other
// four are accents - all taken from the real theme definition so custom
// themes preview correctly.
struct ThemePreviewColors {
    QColor background;      // colors.window.background
    QColor accent;          // colors.accent
    QColor selectedTab;     // colors.tabs.selected.backgrounds.regular
    QColor link;            // colors.messages.textColors.link
    QColor highlightFlash;  // colors.messages.highlightAnimationStart
};

class Theme final
{
public:
    static const std::vector<ThemeDescriptor> builtInThemes;

    static const ThemeDescriptor fallbackTheme;

    static const int AUTO_RELOAD_INTERVAL_MS = 500;

    Theme(const Paths &paths);

    bool isLightTheme() const;
    bool isSystemTheme() const;

    struct TabColors {
        QColor text;
        struct {
            QColor regular;
            QColor hover;
            QColor unfocused;
        } backgrounds;
        struct {
            QColor regular;
            QColor hover;
            QColor unfocused;
        } line;
    };

    struct TextColors {
        QColor regular;
        QColor caret;
        QColor link;
        QColor system;
        QColor chatPlaceholder;
    };

    struct MessageBackgrounds {
        QColor regular;
        QColor alternate;
    };

    QColor accent{"#00aeef"};

    struct {
        QColor background;
        QColor text;
    } window;

    struct {
        TabColors regular;
        TabColors newMessage;
        TabColors highlighted;
        TabColors selected;
        QColor dividerLine;

        QColor liveIndicator;
        QColor rerunIndicator;
    } tabs;

    struct {
        TextColors textColors;
        MessageBackgrounds backgrounds;

        QColor disabled;
        QColor selection;

        QColor highlightAnimationStart;
        QColor highlightAnimationEnd;
    } messages;

    struct {
        TextColors textColors;
        MessageBackgrounds backgrounds;

        QColor disabled;
        QColor selection;
        QColor background;
    } overlayMessages;

    struct {
        QColor background;
        QColor thumb;
        QColor thumbSelected;
    } scrollbars;

    struct {
        QColor messageSeperator;
        QColor background;
        QColor dropPreview;
        QColor dropPreviewBorder;
        QColor dropTargetRect;
        QColor dropTargetRectBorder;
        QColor resizeHandle;
        QColor resizeHandleBackground;

        struct {
            QColor border;
            QColor focusedBorder;
            QColor background;
            QColor focusedBackground;
            QColor text;
            QColor focusedText;
        } header;

        struct {
            QColor background;
            QColor backgroundPulse;
            QColor searchHighlightBackground;
            QColor searchFailText;
            QColor text;

            QString styleSheet;
        } input;
    } splits;

    struct {
        QPixmap copy;
    } buttons;

    QPalette palette;

    void normalizeColor(QColor &color) const;
    void update();

    bool isAutoReloading() const;
    void setAutoReload(bool autoReload);

    // satorino: re-scan <AppData>/Themes for added/removed custom themes
    // without a restart (used by the Themes settings page - docs/SATORINO.md)
    void rescanThemes();

    std::vector<std::pair<QString, QVariant>> availableThemes() const;

    // satorino: preview colors for one theme card on the Themes settings
    // page (docs/SATORINO.md). Colors are read from the theme's own
    // definition - there is no per-theme table, so custom themes preview
    // exactly the way they apply.
    ThemePreviewColors previewColors(const QString &key);

    pajlada::Signals::NoArgSignal updated;

    QStringSetting themeName{"/appearance/theme/name", "Dark"};
    QStringSetting lightSystemThemeName{"/appearance/theme/lightSystem",
                                        "Light"};
    QStringSetting darkSystemThemeName{"/appearance/theme/darkSystem", "Dark"};

private:
    bool isLight_ = false;

    std::vector<ThemeDescriptor> availableThemes_;

    QString currentThemePath_;
    std::unique_ptr<QTimer> themeReloadTimer_;

    QJsonObject currentThemeJson_;

    QObject lifetime_;

    void loadAvailableThemes(const Paths &paths);

    std::optional<ThemeDescriptor> findThemeByKey(const QString &key);

    void parseFrom(const QJsonObject &root, bool isCustomTheme);

    pajlada::Signals::NoArgSignal repaintVisibleChatWidgets_;

    friend class WindowManager;
};

Theme *getTheme();
}  // namespace chatterino
