// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ThemesPage.hpp"

#include "Application.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Theme.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
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
                "<file name>\" in Settings -> Themes and in the theme picker "
                "in Settings -> General. Only .json files are loaded - this "
                "README is ignored.\n"
                "- The theme format is described by "
                "docs/ChatterinoTheme.schema.json in the source repository:\n"
                "  https://github.com/satorino/satorino\n"
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
    auto *layout = new QVBoxLayout(this);

    auto *description = new QLabel(
        "Select a theme - it is applied immediately. Custom themes are .json "
        "files placed in the themes folder in your application data "
        "directory.",
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    this->list_ = new QListWidget(this);
    layout->addWidget(this->list_, 1);

    this->hint_ = new QLabel(this);
    this->hint_->setWordWrap(true);
    this->hint_->setToolTip("Custom themes are loaded from this folder");
    layout->addWidget(this->hint_);

    auto *openButton = new QPushButton("Open themes folder", this);
    openButton->setToolTip(
        "Opens the themes folder in your file manager. If it does not exist "
        "yet it is created and seeded with an example theme and a README.");
    layout->addWidget(openButton, 0, Qt::AlignLeft);

    QObject::connect(this->list_, &QListWidget::currentRowChanged, this,
                     [this](int row) {
                         this->applyTheme(row);
                     });
    QObject::connect(openButton, &QPushButton::clicked, this, [this] {
        this->openThemesFolder();
    });

    this->rebuildThemeList();
}

void ThemesPage::onShow()
{
    // satorino: pick up custom themes added/removed since the last visit
    getTheme()->rescanThemes();
    this->rebuildThemeList();
}

bool ThemesPage::filterElements(const QString &query)
{
    if (query.isEmpty())
    {
        for (int i = 0; i < this->list_->count(); i++)
        {
            this->list_->item(i)->setHidden(false);
        }
        return true;
    }

    bool any = false;
    for (int i = 0; i < this->list_->count(); i++)
    {
        auto *item = this->list_->item(i);
        const bool match = item->text().contains(query, Qt::CaseInsensitive);
        item->setHidden(!match);
        any |= match;
    }
    return any;
}

void ThemesPage::rebuildThemeList()
{
    const QSignalBlocker blocked(this->list_);

    this->list_->clear();

    const auto available = getTheme()->availableThemes();
    const QString currentKey = getTheme()->themeName.getValue();

    int currentRow = -1;
    int row = 0;
    for (const auto &[name, key] : available)
    {
        auto *item = new QListWidgetItem(name, this->list_);
        item->setData(Qt::UserRole, key);
        if (key.toString() == currentKey)
        {
            currentRow = row;
        }
        row++;
    }

    if (currentRow != -1)
    {
        this->list_->setCurrentRow(currentRow);
    }

    this->hint_->setText(QStringLiteral("Themes folder: %1")
                             .arg(getApp()->getPaths().themesDirectory));
}

void ThemesPage::applyTheme(int row)
{
    auto *item = this->list_->item(row);
    if (item == nullptr)
    {
        return;
    }

    // Same setting and apply path as the theme dropdown in Settings ->
    // General: writing themeName triggers Theme::update() which applies the
    // theme live and persists it.
    getTheme()->themeName = item->data(Qt::UserRole).toString();
}

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
