// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
// SPDX-License-Identifier: MIT

#include "singletons/SatorinoStartupUpdate.hpp"

#include "Test.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace chatterino;

TEST(SatorinoStartupUpdate, RequiresNewerStableReleaseWithVerifiedArtifact)
{
    const QString installer = "satorino-windows-x64-setup.exe";
    QJsonObject asset{
        {"name", installer},
        {"state", "uploaded"},
        {"digest", "sha256:" + QString(64, u'a')},
        {"size", 1234},
        {"browser_download_url",
         "https://github.com/whossatori/satorino/releases/download/v2.5.10/" +
             installer},
    };
    QJsonObject release{
        {"tag_name", "v2.5.10"},
        {"draft", false},
        {"prerelease", false},
        {"assets", QJsonArray{asset}},
    };
    const auto select = [&](const QString &current, const QString &name) {
        return satorino::parseReleaseAsset(QJsonDocument(release).toJson(),
                                           current, name);
    };
    const auto valid = select("2.5.9", installer);
    ASSERT_TRUE(valid);
    EXPECT_EQ(valid->size, 1234);
    EXPECT_EQ(valid->sha256, QByteArray(32, char(0xaa)));
    EXPECT_FALSE(select("2.5.10", installer));
    EXPECT_FALSE(select("2.6.0", installer));
    EXPECT_FALSE(select("2.5.9-dev", installer));
    EXPECT_FALSE(select("2.5.9", "satorino-macos-universal.dmg"));
    release["prerelease"] = true;
    EXPECT_FALSE(select("2.5.9", installer));
    release["prerelease"] = false;
    release["draft"] = true;
    EXPECT_FALSE(select("2.5.9", installer));
    release["draft"] = false;
    for (const auto &key : {"digest", "size", "browser_download_url"})
    {
        auto altered = asset;
        altered[key] =
            "https://github.com/another/repo/releases/download/v2.5.10/file";
        release["assets"] = QJsonArray{altered};
        EXPECT_FALSE(select("2.5.9", installer));
    }
}
