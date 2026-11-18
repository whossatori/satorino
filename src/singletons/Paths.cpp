// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/Paths.hpp"

#include "common/Args.hpp"
#include "common/Modes.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

#include <cassert>
#include <stdexcept>

using namespace std::literals;

namespace chatterino {

Paths::Paths(const Args &args, const Modes &modes)
{
    this->initAppFilePathHash();

    this->initRootDirectory(args, modes);
    this->initSubDirectories();
}

bool Paths::createFolder(const QString &folderPath)
{
    return QDir().mkpath(folderPath);
}

QString Paths::cacheDirectory() const
{
    static const auto pathSetting = [] {
        QStringSetting cachePathSetting("/cache/path");

        cachePathSetting.connect([](const auto &newPath) {
            if (!newPath.isEmpty())
            {
                QDir().mkpath(newPath);
            }
        });

        return cachePathSetting;
    }();

    auto path = pathSetting.getValue();

    if (path.isEmpty())
    {
        return this->cacheDirectory_;
    }

    return path;
}

QString Paths::cacheFilePath(const QString &fileName) const
{
    return combinePath(this->cacheDirectory(), fileName);
}

void Paths::initAppFilePathHash()
{
    this->applicationFilePathHash =
        QCryptographicHash::hash(
            QCoreApplication::applicationFilePath().toUtf8(),
            QCryptographicHash::Sha224)
            .toBase64()
            .mid(0, 32)
            .replace("+", "-")
            .replace("/", "x");
}

QString Paths::defaultProfileDirectory()
{
#ifdef Q_OS_WIN
    const auto appData = qEnvironmentVariable("APPDATA");
    return QDir::isAbsolutePath(appData) ? combinePath(appData, "Chatterino2")
                                         : QString{};
#else
    // satorino: keep the Chatterino profile path independent of the app name.
    const auto base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#    if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    constexpr auto profileName = "chatterino";
#    else
    constexpr auto profileName = "satorino";
#    endif
    return base.isEmpty() ? QString{} : combinePath(base, profileName);
#endif
}

void Paths::initRootDirectory([[maybe_unused]] const Args &args,
                              [[maybe_unused]] const Modes &modes)
{
#ifdef Q_OS_WIN
    // satorino: both Windows distributions open the existing shared profile.
    this->rootAppDataDirectory = defaultProfileDirectory();
    if (this->rootAppDataDirectory.isEmpty())
    {
        throw std::runtime_error("Cannot locate the Chatterino2 profile "
                                 "because APPDATA is unavailable.");
    }
#else
    this->rootAppDataDirectory = [&]() -> QString {
        if (modes.isPortable)
        {
            // override
            if (args.portableDirectory.has_value())
            {
                return args.portableDirectory.value();
            }

            return QCoreApplication::applicationDirPath();
        }

        const auto profile = defaultProfileDirectory();
        if (profile.isEmpty())
        {
            throw std::runtime_error("Cannot locate the Chatterino profile.");
        }
        return profile;
    }();
#endif
}

void Paths::initSubDirectories()
{
    assert(!this->rootAppDataDirectory.isEmpty());

    auto makePath = [&](const QString &name) -> QString {
        auto path = combinePath(this->rootAppDataDirectory, name);

        if (!QDir().mkpath(path))
        {
            throw std::runtime_error("Could not create directory \""s +
                                     path.toStdString() + "\"");
        }

        return path;
    };

    makePath("");
    this->settingsDirectory = makePath("Settings");
    this->cacheDirectory_ = makePath("Cache");
    this->messageLogDirectory = makePath("Logs");
    this->miscDirectory = makePath("Misc");
    this->twitchProfileAvatars =
        makePath(combinePath("ProfileAvatars", "twitch"));
    this->pluginsDirectory = makePath("Plugins");
    this->themesDirectory = makePath("Themes");
    this->crashdumpDirectory = makePath("Crashes");
    this->dictionariesDirectory = makePath("Dictionaries");
#ifdef Q_OS_WIN
    this->ipcDirectory = makePath("IPC");
#else

    this->ipcDirectory = "/tmp";
#endif
}

}  // namespace chatterino
