// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <optional>

namespace chatterino {
class Args;
class Modes;
class Paths;
class Settings;

namespace satorino {
struct ReleaseAsset {
    QUrl url;
    QByteArray sha256;
    qint64 size;
};

// Accept only a newer stable release and its matching GitHub release asset.
std::optional<ReleaseAsset> parseReleaseAsset(const QByteArray &response,
                                              const QString &version,
                                              const QString &assetName);

// True once the startup update has handed off to a replacement process.
bool tryStartupUpdate(const Args &args, const Modes &modes, const Paths &paths,
                      const Settings &settings);
}  // namespace satorino
}  // namespace chatterino
