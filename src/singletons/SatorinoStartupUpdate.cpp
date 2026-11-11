// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
// SPDX-License-Identifier: MIT

#include "singletons/SatorinoStartupUpdate.hpp"

#include "common/Args.hpp"
#include "common/Modes.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "RunGui.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QVersionNumber>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#    include <stdio.h>
#endif

#ifdef Q_OS_LINUX
#    include <unistd.h>
#endif

namespace chatterino::satorino {
namespace {
constexpr auto RELEASE_API =
    "https://api.github.com/repos/whossatori/satorino/releases/latest";
constexpr auto DOWNLOAD_PREFIX =
    "https://github.com/whossatori/satorino/releases/download/";
constexpr auto SKIP_ARGUMENT = "--satorino-skip-update";
constexpr qint64 MAX_RELEASE_BYTES = 512 * 1024;
constexpr qint64 MAX_ASSET_BYTES = 256 * 1024 * 1024;
constexpr int CHECK_TIMEOUT_MS = 1500;
constexpr int DOWNLOAD_TIMEOUT_MS = 120000;

bool isHttps(const QUrl &url)
{
    return url.isValid() && url.scheme() == "https" && url.port(443) == 443 &&
           url.userInfo().isEmpty() && !url.hasFragment();
}

bool isDownloadRedirect(const QUrl &url)
{
    return isHttps(url) &&
           (url.host() == "release-assets.githubusercontent.com" ||
            url.host() == "objects.githubusercontent.com");
}

bool get(QNetworkAccessManager &network, const QUrl &url, QIODevice &output,
         qint64 limit, int timeout, QProgressDialog *progress = nullptr,
         QCryptographicHash *hash = nullptr)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Satorino");
    request.setRawHeader("Accept-Encoding", "identity");
    if (hash == nullptr)
    {
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         hash == nullptr
                             ? QNetworkRequest::ManualRedirectPolicy
                             : QNetworkRequest::UserVerifiedRedirectPolicy);
    auto *reply = network.get(request);
    reply->setReadBufferSize(128 * 1024);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    qint64 received = 0;
    bool failed = false;
    auto abort = [&] {
        failed = true;
        reply->abort();
    };
    const auto read = [&] {
        if (failed || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                              .toInt() != 200)
        {
            // A redirect response body does not belong to the update.
            reply->readAll();
            return;
        }
        const auto bytes = reply->readAll();
        if (bytes.size() > limit - received ||
            output.write(bytes) != bytes.size())
        {
            abort();
            return;
        }
        received += bytes.size();
        if (hash != nullptr)
        {
            hash->addData(bytes);
        }
        if (progress != nullptr)
        {
            progress->setValue(static_cast<int>(received));
        }
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, abort);
    QObject::connect(
        reply, &QNetworkReply::redirected, &loop, [&](const QUrl &location) {
            if (isDownloadRedirect(reply->url().resolved(location)))
            {
                reply->redirectAllowed();
            }
            else
            {
                abort();
            }
        });
    if (progress != nullptr)
    {
        QObject::connect(progress, &QProgressDialog::canceled, &loop, abort);
    }
    timer.start(timeout);
    if (!reply->isFinished())
    {
        loop.exec();
    }
    read();
    const auto status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = !failed && status == 200 &&
                    reply->error() == QNetworkReply::NoError &&
                    (hash == nullptr || received == limit);
    // Disconnect callbacks that reference this stack before releasing the reply.
    delete reply;
    return ok;
}

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
QStringList relaunchArguments()
{
    auto arguments = QCoreApplication::arguments().mid(1);
    arguments.prepend(SKIP_ARGUMENT);
    return arguments;
}
#endif

#ifdef Q_OS_WIN
bool install(const QString &file, const QFileInfo &target, QTemporaryDir &stage)
{
    const QStringList arguments{
        "/SP-",
        "/VERYSILENT",
        "/SUPPRESSMSGBOXES",
        "/NORESTART",
        "/NOCLOSEAPPLICATIONS",
        "/NORESTARTAPPLICATIONS",
        "/DIR=" + target.absolutePath(),
        "/SATORINOPID=" + QString::number(QCoreApplication::applicationPid())};
    if (!QProcess::startDetached(file, arguments))
    {
        return false;
    }
    stage.setAutoRemove(false);  // The installer is now using the file.
    return true;
}
#endif

#ifdef Q_OS_LINUX
bool install(const QString &file, const QFileInfo &target, QTemporaryDir &stage)
{
    const auto original = QFile::encodeName(target.absoluteFilePath());
    const auto previous =
        QFile::encodeName(stage.filePath("previous.AppImage"));
    if (!QFile::setPermissions(file, target.permissions()) ||
        ::link(original.constData(), previous.constData()) != 0 ||
        ::rename(QFile::encodeName(file).constData(), original.constData()) !=
            0)
    {
        return false;
    }

    // A child AppImage must mount its own files, not inherit the old mount.
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto &key : {"APPIMAGE", "APPDIR", "OWD", "LD_LIBRARY_PATH",
                            "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH"})
    {
        environment.remove(key);
    }
    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProgram(target.absoluteFilePath());
    process.setWorkingDirectory(target.absolutePath());
    process.setArguments(relaunchArguments());
    if (process.startDetached())
    {
        return true;
    }
    if (::rename(previous.constData(), original.constData()) != 0)
    {
        stage.setAutoRemove(false);
        qCWarning(chatterinoUpdate) << "Previous AppImage saved at" << previous;
    }
    return false;
}
#endif

#ifdef Q_OS_MACOS
bool runTool(const QString &program, const QStringList &arguments, int timeout,
             QByteArray *output = nullptr)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(timeout))
    {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    if (output != nullptr)
    {
        *output = process.readAllStandardOutput().trimmed();
    }
    return process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
}

bool install(const QString &file, const QFileInfo &target, QTemporaryDir &stage)
{
    const auto mount = stage.filePath("mounted");
    const auto staged = stage.filePath("Satorino.app");
    if (!QDir().mkpath(mount))
    {
        return false;
    }
    stage.setAutoRemove(false);
    if (!runTool(
            "/usr/bin/hdiutil",
            {"attach", "-readonly", "-nobrowse", "-mountpoint", mount, file},
            30000))
    {
        if (runTool("/usr/bin/hdiutil", {"detach", mount}, 10000))
        {
            stage.setAutoRemove(true);
        }
        return false;
    }
    const auto source = QDir(mount).filePath("Satorino.app");
    const bool copied = runTool("/usr/bin/ditto", {source, staged}, 30000);
    if (!runTool("/usr/bin/hdiutil", {"detach", mount}, 10000))
    {
        return false;
    }
    stage.setAutoRemove(true);
    QByteArray id;
    if (!copied ||
        !runTool("/usr/libexec/PlistBuddy",
                 {"-c", "Print :CFBundleIdentifier",
                  QDir(staged).filePath("Contents/Info.plist")},
                 5000, &id) ||
        id != "cc.satos.Satorino" ||
        !runTool("/usr/bin/codesign",
                 {"--verify", "--deep", "--strict", staged}, 15000) ||
        !runTool("/usr/sbin/spctl", {"--assess", "--type", "execute", staged},
                 15000))
    {
        return false;
    }
    const auto original = QFile::encodeName(target.absoluteFilePath());
    const auto replacement = QFile::encodeName(staged);
    if (::renamex_np(original.constData(), replacement.constData(),
                     RENAME_SWAP) != 0)
    {
        return false;
    }
    auto arguments =
        QStringList{"-n", "-a", target.absoluteFilePath(), "--args"};
    arguments.append(relaunchArguments());
    if (runTool("/usr/bin/open", arguments, 10000))
    {
        return true;
    }
    if (::renamex_np(original.constData(), replacement.constData(),
                     RENAME_SWAP) != 0)
    {
        stage.setAutoRemove(false);
        qCWarning(chatterinoUpdate) << "Previous app bundle saved at" << staged;
    }
    return false;
}
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
QString targetPath()
{
    const QFileInfo exe(QCoreApplication::applicationFilePath());
#    ifdef Q_OS_WIN
    return QFileInfo::exists(
               QDir(exe.absolutePath()).filePath("satorino-installed"))
               ? exe.absoluteFilePath()
               : QString{};
#    elif defined(Q_OS_LINUX)
    const QFileInfo image(qEnvironmentVariable("APPIMAGE"));
    const auto appDir = qEnvironmentVariable("APPDIR");
    return image.isAbsolute() && image.isFile() && !appDir.isEmpty() &&
                   exe.absoluteFilePath().startsWith(
                       QDir(appDir).absolutePath() + '/')
               ? image.absoluteFilePath()
               : QString{};
#    elif defined(Q_OS_MACOS)
    QDir bundle(exe.absolutePath());
    if (bundle.dirName() != "MacOS" || !bundle.cdUp() ||
        bundle.dirName() != "Contents" || !bundle.cdUp() ||
        !bundle.dirName().endsWith(".app") ||
        bundle.absolutePath().contains("/AppTranslocation/") ||
        bundle.absolutePath().startsWith("/Volumes/"))
    {
        return {};
    }
    return bundle.absolutePath();
#    endif
}
#endif
}  // namespace

std::optional<ReleaseAsset> parseReleaseAsset(const QByteArray &response,
                                              const QString &version,
                                              const QString &assetName)
{
    const auto release = QJsonDocument::fromJson(response).object();
    const auto tag = release.value("tag_name").toString();
    static const QRegularExpression stableTag(QStringLiteral(
        R"(\Av(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})\.(?:0|[1-9][0-9]{0,8})\z)"));
    static const QRegularExpression digestPattern(
        QStringLiteral(R"(\Asha256:[0-9a-fA-F]{64}\z)"));
    if (release.value("draft").toBool(true) ||
        release.value("prerelease").toBool(true) ||
        !stableTag.match(tag).hasMatch() ||
        !stableTag.match("v" + version).hasMatch() ||
        QVersionNumber::fromString(tag.mid(1)) <=
            QVersionNumber::fromString(version))
    {
        return std::nullopt;
    }
    const QUrl expected(QString(DOWNLOAD_PREFIX) + tag + '/' + assetName);
    const auto assets = release.value("assets").toArray();
    for (const auto &item : assets)
    {
        const auto asset = item.toObject();
        const auto digest = asset.value("digest").toString();
        const auto size = asset.value("size").toInteger();
        if (asset.value("name").toString() == assetName &&
            asset.value("state").toString() == "uploaded" &&
            asset.value("browser_download_url").toString() ==
                expected.toString() &&
            digestPattern.match(digest).hasMatch() && size > 0 &&
            size <= MAX_ASSET_BYTES)
        {
            return ReleaseAsset{
                expected, QByteArray::fromHex(digest.mid(7).toLatin1()), size};
        }
    }
    return std::nullopt;
}

bool tryStartupUpdate(const Args &args, const Modes &modes, const Paths &paths,
                      const Settings &settings)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (args.skipSatorinoStartupUpdate || args.isFramelessEmbed ||
        args.safeMode || args.crashRecovery || args.remoteRestart ||
        !settings.satorinoAutoUpdate.getValue() || modes.isPortable ||
        modes.isExternallyPackaged || Version::instance().isFlatpak() ||
        Version::instance().isNightly() || Version::instance().isModified())
    {
        return false;
    }
    const QFileInfo target(targetPath());
#    if defined(Q_OS_LINUX) && !defined(Q_PROCESSOR_X86_64)
    return false;
#    endif
    if (!target.exists() || target.isSymLink() ||
        !QFileInfo(target.absolutePath()).isWritable() ||
        activateExistingGuiInstance(paths, false))
    {
        return false;
    }

    QLockFile lock(
        QDir(paths.miscDirectory).filePath("satorino-startup-update.lock"));
    if (!lock.tryLock(0))
    {
        return false;
    }

#    ifdef Q_OS_WIN
    const QString assetName = "satorino-windows-x64-setup.exe";
    const auto stageRoot = QDir::tempPath();
#    elif defined(Q_OS_LINUX)
    const QString assetName = "satorino-linux-x86_64.AppImage";
    const auto stageRoot = target.absolutePath();
#    elif defined(Q_OS_MACOS)
    const QString assetName = "satorino-macos-universal.dmg";
    const auto stageRoot = target.absolutePath();
#    endif
    QNetworkAccessManager network;
    QBuffer metadata;
    metadata.open(QIODevice::WriteOnly);
    if (!get(network, QUrl(RELEASE_API), metadata, MAX_RELEASE_BYTES,
             CHECK_TIMEOUT_MS))
    {
        return false;
    }
    const auto asset = parseReleaseAsset(
        metadata.data(), Version::instance().version(), assetName);
    if (!asset)
    {
        return false;
    }
    QTemporaryDir stage(QDir(stageRoot).filePath(".satorino-update-XXXXXX"));
    if (!stage.isValid())
    {
        return false;
    }
    const auto file = stage.filePath(assetName);
    QSaveFile output(file);
    if (!output.open(QIODevice::WriteOnly))
    {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QProgressDialog progress("Downloading a Satorino update...", "Skip", 0,
                             static_cast<int>(asset->size));
    progress.setAttribute(Qt::WA_QuitOnClose, false);
    progress.setWindowTitle("Satorino update");
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    if (!get(network, asset->url, output, asset->size, DOWNLOAD_TIMEOUT_MS,
             &progress, &hash) ||
        progress.wasCanceled() || hash.result() != asset->sha256 ||
        !output.commit() || activateExistingGuiInstance(paths, false))
    {
        return false;
    }
    progress.setLabelText("Installing the Satorino update...");
    progress.setCancelButton(nullptr);
    QCoreApplication::processEvents();
    return install(file, target, stage);
#else
    return false;
#endif
}
}  // namespace chatterino::satorino
