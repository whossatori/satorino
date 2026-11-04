// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ThemesPage.hpp"

#include "Application.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Theme.hpp"
#include "widgets/layout/FlowLayout.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include "widgets/settingspages/ThemeCard.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace chatterino {

namespace {

// The example theme seeded into <AppData>/Themes - a verbatim copy of the
// built-in Dark theme, so users always have a valid starting point.
constexpr auto *EXAMPLE_THEME_FILE = "satorino-example.json";
constexpr auto *THEMES_README_FILE = "README.txt";

void seedThemesDirectory(const QString &directory)
{
    QDir themesDir(directory);

    const auto examplePath = themesDir.filePath(EXAMPLE_THEME_FILE);
    if (!QFile::exists(examplePath))
    {
        QFile::copy(":/themes/Dark.json", examplePath);
        // make sure the seeded copy is editable by the user
        QFile::setPermissions(examplePath,
                              QFile::ReadOwner | QFile::WriteOwner |
                                  QFile::ReadUser | QFile::WriteUser |
                                  QFile::ReadGroup | QFile::ReadOther);
    }

    const auto readmePath = themesDir.filePath(THEMES_README_FILE);
    if (!QFile::exists(readmePath))
    {
        QFile readme(readmePath);
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            readme.write(
                "This folder holds custom themes for satorino.\n"
                "\n"
                "- Every *.json file in this folder shows up as \"Custom: "
                "<file name>\" in Settings -> Themes. Only .json files are "
                "loaded - this README is ignored.\n"
                "- The theme format is described by "
                "docs/ChatterinoTheme.schema.json in the source repository:\n"
                "  https://github.com/whossatori/satorino\n"
                "- satorino-example.json is a copy of the built-in Dark theme "
                "- edit it as a starting point.\n"
                "- Colors missing from a custom theme fall back to the "
                "built-in theme named by metadata.fallbackTheme; broken JSON "
                "files are skipped (the current theme stays active).\n");
        }
    }
}

}  // namespace

ThemesPage::ThemesPage()
{
    this->setStyleSheet("#themesScrollContent { background: #222; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scroll);

    auto *content = new QWidget;
    content->setObjectName("themesScrollContent");

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 12, 16, 16);
    contentLayout->setSpacing(12);

    auto *description = new QLabel(
        "Select a theme - it is applied immediately. Custom themes are .json "
        "files placed in the themes folder in your application data "
        "directory.",
        content);
    description->setWordWrap(true);
    description->setStyleSheet("color: #999;");
    contentLayout->addWidget(description);

    // the same section header style the other settings pages use -
    // settings.qss paints TitleLabel with the dialog accent (#4FC3F7)
    contentLayout->addWidget(new TitleLabel(QStringLiteral("Theme:")));

    this->flow_ = new FlowLayout({0, 10, 12});
    contentLayout->addLayout(this->flow_);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // the adaptive system theme dropdowns moved from Settings -> General
    // (same setting keys, same behaviour) live under the grid, under a
    // SubtitleLabel like the subsections on the General page
    this->systemLayout_ = new QVBoxLayout;
    this->systemLayout_->setSpacing(8);
    this->systemLayout_->addWidget(
        new SubtitleLabel(QStringLiteral("Adaptive system theme:")));
    contentLayout->addLayout(this->systemLayout_);
#endif

    this->hint_ = new QLabel(content);
    this->hint_->setWordWrap(true);
    this->hint_->setStyleSheet("color: #999;");
    this->hint_->setToolTip("Custom themes are loaded from this folder");
    contentLayout->addWidget(this->hint_);

    auto *openButton = new QPushButton("Open themes folder", content);
    openButton->setToolTip(
        "Opens the themes folder in your file manager. If it does not exist "
        "yet it is created and seeded with an example theme and a README.");
    contentLayout->addWidget(openButton, 0, Qt::AlignLeft);

    contentLayout->addStretch(1);

    scroll->setWidget(content);

    QObject::connect(openButton, &QPushButton::clicked, this, [this] {
        this->openThemesFolder();
    });

    // The highlighted card follows the theme setting in both directions:
    // clicking a card writes the setting (see addCard), and any other
    // writer (/settheme, ...) updates the grid through this connection.
    getTheme()->themeName.connect(
        [this](const auto &) {
            this->updateSelectedCard();
        },
        this->managedConnections_);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // the System card previews the dark/light system theme pair - rebuild
    // it when either setting changes
    getTheme()->darkSystemThemeName.connect(
        [this](const auto &) {
            this->rebuildThemeList();
        },
        this->managedConnections_);
    getTheme()->lightSystemThemeName.connect(
        [this](const auto &) {
            this->rebuildThemeList();
        },
        this->managedConnections_);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    this->rebuildSystemThemeSelectors();
#endif

    this->rebuildThemeList();
}

void ThemesPage::onShow()
{
    // satorino: pick up custom themes added/removed since the last visit
    getTheme()->rescanThemes();
    this->rebuildThemeList();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    this->rebuildSystemThemeSelectors();
#endif
}

bool ThemesPage::filterElements(const QString &query)
{
    // Cards are taken out of the flow layout and only the matching ones are
    // re-added - FlowLayout reserves space for hidden widgets, so removing
    // them from the layout is what collapses the gaps.
    while (auto *item = this->flow_->takeAt(0))
    {
        delete item;
    }

    bool any = false;
    for (auto *card : this->cards_)
    {
        const bool match = query.isEmpty() ||
                           card->title().contains(query, Qt::CaseInsensitive);
        if (match)
        {
            this->flow_->addWidget(card);
            card->show();
            any = true;
        }
        else
        {
            card->hide();
        }
    }

    if (!any && !query.isEmpty())
    {
        // keep the page findable through its other controls (the folder
        // hint, the description, ...)
        for (auto *label : this->findChildren<QLabel *>())
        {
            if (label->text().contains(query, Qt::CaseInsensitive))
            {
                any = true;
                break;
            }
        }
    }

    return any;
}

void ThemesPage::addCard(const QString &title, const QString &key,
                         const ThemePreviewColors &primary,
                         const std::optional<ThemePreviewColors> &secondary,
                         const QString &tooltip)
{
    auto *card = new ThemeCard(title, key, primary, secondary);
    if (!tooltip.isEmpty())
    {
        card->setToolTip(tooltip);
    }
    this->flow_->addWidget(card);
    this->cards_.push_back(card);

    QObject::connect(card, &ThemeCard::activated, this, [this, key] {
        // the exact same setting write /settheme performs - writing
        // themeName triggers Theme::update(), which applies the theme live
        // and persists it
        getTheme()->themeName = key;
    });
}

void ThemesPage::rebuildThemeList()
{
    while (auto *item = this->flow_->takeAt(0))
    {
        if (auto *card = item->widget())
        {
            card->deleteLater();
        }
        delete item;
    }
    this->cards_.clear();

    const auto available = getTheme()->availableThemes();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // the adaptive card comes first, like the "System" entry of the list
    // it replaces
    this->addCard(
        QStringLiteral("System"), QStringLiteral("System"),
        getTheme()->previewColors(getTheme()->darkSystemThemeName.getValue()),
        getTheme()->previewColors(getTheme()->lightSystemThemeName.getValue()),
        QStringLiteral("Automatically uses the dark or light system theme "
                       "that matches your system's colour scheme."));
#endif

    for (const auto &[name, key] : available)
    {
        this->addCard(name, key.toString(),
                      getTheme()->previewColors(key.toString()));
    }

    this->updateSelectedCard();

    this->hint_->setText(QStringLiteral("Themes folder: %1")
                             .arg(getApp()->getPaths().themesDirectory));
}

void ThemesPage::updateSelectedCard()
{
    const auto currentKey = getTheme()->themeName.getValue();
    for (auto *card : this->cards_)
    {
        card->setSelected(card->key() == currentKey);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
void ThemesPage::rebuildSystemThemeSelectors()
{
    // (re)builds the adaptive system theme dropdowns that used to live on
    // Settings -> General - same setting keys, same behaviour, same
    // conditionallyEnabledBy semantics. Rebuilding picks up custom themes
    // added since the last build.
    for (auto *widget : this->systemSelectors_)
    {
        widget->deleteLater();
    }
    this->systemSelectors_.clear();

    auto *themes = getTheme();

    this->systemSelectors_.push_back(
        SettingWidget::dropdown("Dark system theme",
                                themes->darkSystemThemeName,
                                themes->availableThemes())
            ->setTooltip("This theme is selected if your system is in a dark "
                         "theme and you enabled the adaptive 'System' theme.")
            ->conditionallyEnabledBy(themes->themeName, "System"));

    this->systemSelectors_.push_back(
        SettingWidget::dropdown("Light system theme",
                                themes->lightSystemThemeName,
                                themes->availableThemes())
            ->setTooltip("This theme is selected if your system is in a light "
                         "theme and you enabled the adaptive 'System' theme.")
            ->conditionallyEnabledBy(themes->themeName, "System"));

    for (auto *widget : this->systemSelectors_)
    {
        this->systemLayout_->addWidget(widget);
    }
}
#endif

void ThemesPage::openThemesFolder()
{
    const auto &paths = getApp()->getPaths();

    QDir().mkpath(paths.themesDirectory);
    seedThemesDirectory(paths.themesDirectory);

    getTheme()->rescanThemes();
    this->rebuildThemeList();

    QDesktopServices::openUrl(QUrl::fromLocalFile(paths.themesDirectory));
}

}  // namespace chatterino
