// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "BrowserExtension.hpp"
#include "common/Args.hpp"
#include "common/Env.hpp"
#include "common/Modes.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "providers/IvrApi.hpp"
#include "providers/NetworkConfigurationProvider.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "RunGui.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/FileLogger.hpp"
#include "singletons/Paths.hpp"
#include "singletons/SatorinoStartupUpdate.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Updates.hpp"
#include "util/AttachToConsole.hpp"
#include "util/CombinePath.hpp"
#include "util/IpcQueue.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSslSocket>
#include <QStringList>
#include <Qt>
#include <QtCore/QtPlugin>
#ifdef Q_OS_WIN
#    include <shobjidl_core.h>
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>

#include <cstring>
#include <iostream>
#include <memory>

#ifdef CHATTERINO_WITH_AVIF_PLUGIN
Q_IMPORT_PLUGIN(QAVIFPlugin)
#endif

using namespace chatterino;

namespace {

bool argvRequestsLegacyScaling(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] != nullptr &&
            std::strcmp(argv[i], "--use-old-scaling") == 0)
        {
            return true;
        }
    }
    return false;
}

bool jsonPointerBoolIsTrue(const QString &filePath, const char *jsonPointerPath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
    {
        return false;
    }
    const QByteArray data = f.readAll();
    rapidjson::Document doc;
    if (doc.Parse(data.constData(), data.size()).HasParseError())
    {
        return false;
    }
    const auto *v = rapidjson::Pointer(jsonPointerPath).Get(doc);
    return v != nullptr && v->IsBool() && v->GetBool();
}

// Find possible `settings.json` locations before QApplication is created
QStringList legacyScalingSettingsJsonCandidates([[maybe_unused]] int argc,
                                                [[maybe_unused]] char **argv)
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    const auto profile = Paths::windowsProfileDirectory();
    if (!profile.isEmpty())
    {
        candidates << combinePath(profile,
                                  QStringLiteral("Settings/settings.json"));
    }
#else
    const QFileInfo fi(QString::fromLocal8Bit(argc > 0 ? argv[0] : ""));
    const QString appDir = fi.absoluteDir().absolutePath();
    QFile modesFile(combinePath(appDir, QStringLiteral("modes")));
    bool portable =
        QFile::exists(combinePath(appDir, QStringLiteral("portable")));
    if (modesFile.open(QIODevice::ReadOnly))
    {
        while (!portable && !modesFile.atEnd())
        {
            portable = modesFile.readLine().trimmed() == "portable";
        }
    }
    if (portable)
    {
        candidates << combinePath(appDir,
                                  QStringLiteral("Settings/settings.json"));
        return candidates;
    }

#    if defined(Q_OS_MACOS)
    const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
    if (!home.isEmpty())
    {
        candidates << combinePath(
            combinePath(home,
                        QStringLiteral("Library/Application Support/satorino")),
            QStringLiteral("Settings/settings.json"));
    }
#    else
    const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
    if (!home.isEmpty())
    {
        QString xdgData = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
        if (xdgData.isEmpty())
        {
            xdgData = combinePath(home, QStringLiteral(".local/share"));
        }
        candidates << combinePath(
            combinePath(xdgData, QStringLiteral("satorino")),
            QStringLiteral("Settings/settings.json"));
    }
#    endif
#endif
    return candidates;
}

bool settingsFileRequestsLegacyScaling(int argc, char **argv)
{
    for (const auto &path : legacyScalingSettingsJsonCandidates(argc, argv))
    {
        if (jsonPointerBoolIsTrue(path, "/appearance/useLegacyScaling"))
        {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argvRequestsLegacyScaling(argc, argv) ||
        settingsFileRequestsLegacyScaling(argc, argv))
    {
        QApplication::setAttribute(Qt::AA_Use96Dpi, true);
    }

    QApplication a(argc, argv);

    // satorino: Keep the application identity separate from profile selection.
    QCoreApplication::setApplicationName("satorino");
    QCoreApplication::setApplicationVersion(CHATTERINO_VERSION);
    QCoreApplication::setOrganizationDomain("satos.cc");
    // Must match the installed .desktop / Flatpak app-id so the shell
    // associates this window with satorino's icon (not Chatterino's).
    // satorino: app id (was com.leafyzito.leafyrino)
    QGuiApplication::setDesktopFileName("cc.satos.Satorino");
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(
        Version::instance().appUserModelID().c_str());
#endif

    const Args args(a);
    const Modes modes(args);
    std::unique_ptr<Paths> paths;

    // Optional logger override that logs to a file
    FileLogger logger;

    try
    {
        paths = std::make_unique<Paths>(args, modes);
    }
    catch (std::runtime_error &error)
    {
        QMessageBox box;
        box.setText(error.what());
#ifndef Q_OS_WIN
        if (modes.isPortable)
        {
            auto errorMessage =
                error.what() +
                QStringLiteral(
                    "\n\nInfo: Portable mode requires the application to "
                    "be in a writeable location. If you don't want "
                    "portable mode reinstall the application. "
                    "https://chatterino.com.");
            std::cerr << errorMessage.toLocal8Bit().constData() << '\n';
            std::cerr.flush();
            box.setText(errorMessage);
        }
#endif
        box.exec();
        return 1;
    }
    ipc::initPaths(paths.get());

#ifdef CHATTERINO_WITH_CRASHPAD
    const auto crashpadHandler = installCrashHandler(args, *paths);
#endif

    if (args.shouldRunBrowserExtensionHost)
    {
#ifdef Q_OS_MACOS
        ::chatterinoSetMacOsActivationPolicyProhibited();
#endif
        runBrowserExtensionHost();
    }
    else if (args.printVersion)
    {
        attachToConsole();

        auto version = Version::instance();
        auto versionMessage =
            QString("%1 (commit %2%3)")
                .arg(version.fullVersion())
                .arg(version.commitHash())
                .arg(version.isNightly() ? ", " + version.dateOfBuild() : "");
        std::cout << versionMessage.toLocal8Bit().constData() << '\n';
        std::cout.flush();
    }
    else
    {
        if (args.verbose)
        {
            attachToConsole();
        }

        qCInfo(chatterinoApp).noquote()
            << "satorino Qt SSL library build version:"
            << QSslSocket::sslLibraryBuildVersionString();
        qCInfo(chatterinoApp).noquote()
            << "satorino Qt SSL library version:"
            << QSslSocket::sslLibraryVersionString();
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
        qCInfo(chatterinoApp).noquote()
            << "satorino Qt SSL active backend:" << QSslSocket::activeBackend()
            << "of" << QSslSocket::availableBackends().join(", ");
#    if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        qCInfo(chatterinoApp) << "satorino Qt SSL active backend features:"
                              << QSslSocket::supportedFeatures();
#    endif
        qCInfo(chatterinoApp) << "satorino Qt SSL active backend protocols:"
                              << QSslSocket::supportedProtocols();
#endif

        Settings settings(modes, args, paths->settingsDirectory);
#ifndef Q_OS_MACOS
        if (!args.remoteRestart && !args.isFramelessEmbed &&
            settings.trayHideOnClose.getValue() &&
            activateExistingGuiInstance(*paths))
        {
            return 0;
        }
#endif

        if (satorino::tryStartupUpdate(args, modes, *paths, settings))
        {
            return 0;
        }

        Updates updates(modes, *paths, settings);

        NetworkConfigurationProvider::applyFromEnv(Env::get());

        IvrApi::initialize();
        Helix::initialize();

        runGui(a, modes, *paths, settings, args, updates);
    }
    return 0;
}
