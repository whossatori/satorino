// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/spellcheck/SpellChecker.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"
#include "util/FilesystemHelpers.hpp"
#include "util/XDGDirectory.hpp"

#ifdef CHATTERINO_WITH_SPELLCHECK
#    include <hunspell/hunspell.hxx>
#    include <QDir>
#    include <QFile>
#    include <QFileInfo>
#    include <QStringBuilder>
#    include <QTemporaryDir>

#    include <algorithm>
#    include <tuple>
#    include <utility>
#endif

namespace chatterino {

#ifdef CHATTERINO_WITH_SPELLCHECK

namespace {

// satorino: the packaged dictionary uses the same path setting as other dictionaries.
constexpr auto BUILT_IN_DICTIONARY = ":/dictionaries/en_US";

std::vector<DictionaryInfo> loadDictionariesFromDirectory(
    const QDir &searchDirectory, bool isSystem)
{
    std::vector<DictionaryInfo> dictionaries;

    for (const auto &affInfo : searchDirectory.entryInfoList(
             {"*.aff"}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (!affInfo.isFile())
        {
            continue;
        }

        auto dictName = affInfo.baseName();

        auto dicInfo = QFileInfo(searchDirectory, dictName % ".dic");
        if (!dicInfo.isFile())
        {
            continue;
        }

        auto isSymbolicLink =
            affInfo.isSymbolicLink() || dicInfo.isSymbolicLink();

        QString path = [&] {
            if (isSystem)
            {
                return searchDirectory.absoluteFilePath(dictName);
            }
            return dictName;
        }();

        dictionaries.push_back(DictionaryInfo{
            .name = dictName,
            .path = path,
            .isSymbolicLink = isSymbolicLink,
            .isSystem = isSystem,
        });
    }

    return dictionaries;
}

QString resolveDictionaryPath(const QString &path)
{
    if (QDir::isAbsolutePath(path))
    {
        return path;
    }
    return combinePath(getApp()->getPaths().dictionariesDirectory, path);
}

// satorino: Hunspell's MSVC build uses UTF-8 for prefixed Windows paths.
std::string hunspellPath(const std::filesystem::path &path)
{
#    if defined(Q_OS_WIN) && defined(_MSC_VER)
    auto native = QDir::toNativeSeparators(stdPathToQString(path));
    if (native.startsWith(u"\\\\?\\"))
    {
        return native.toStdString();
    }
    if (native.startsWith(u"\\\\"))
    {
        return (QStringLiteral("\\\\?\\UNC\\") + native.sliced(2))
            .toStdString();
    }
    return (QStringLiteral("\\\\?\\") + native).toStdString();
#    else
    return path.string();
#    endif
}

}  // namespace

class SpellCheckerPrivate
{
public:
    static std::unique_ptr<SpellCheckerPrivate> tryLoad(
        const QString &path = {});

private:
    SpellCheckerPrivate(std::unique_ptr<QTemporaryDir> dictionaryDirectory,
                        const char *affpath, const char *dpath);

    // Keep the extracted files alive until after Hunspell is destroyed.
    std::unique_ptr<QTemporaryDir> dictionaryDirectory_;

public:
    Hunspell hunspell;
};

std::unique_ptr<SpellCheckerPrivate> SpellCheckerPrivate::tryLoad(
    const QString &path)
{
    if (path.isEmpty())
    {
        qCDebug(chatterinoSpellcheck) << "No path specified";
        return nullptr;
    }

    auto resolvedPath = resolveDictionaryPath(path);
    std::unique_ptr<QTemporaryDir> dictionaryDirectory;
    if (path == QLatin1String(BUILT_IN_DICTIONARY))
    {
        // Hunspell opens native files, so extract the embedded dictionary first.
        dictionaryDirectory = std::make_unique<QTemporaryDir>();
        if (!dictionaryDirectory->isValid() ||
            !QFile::copy(path % ".aff",
                         dictionaryDirectory->filePath("en_US.aff")) ||
            !QFile::copy(path % ".dic",
                         dictionaryDirectory->filePath("en_US.dic")))
        {
            qCWarning(chatterinoSpellcheck)
                << "Failed to extract the built-in English dictionary";
            return nullptr;
        }
        resolvedPath = dictionaryDirectory->filePath("en_US");
    }
    auto aff = qStringToStdPath(resolvedPath % ".aff");
    auto dic = qStringToStdPath(resolvedPath % ".dic");

    if (!std::filesystem::exists(aff) || !std::filesystem::exists(dic))
    {
        qCInfo(chatterinoSpellcheck).nospace().noquote()
            << "Failed to find " << resolvedPath << ".{aff,dic}";
        return nullptr;
    }
    std::error_code ec;
    auto affCanonical = std::filesystem::weakly_canonical(aff, ec);
    if (ec)
    {
        qCInfo(chatterinoSpellcheck)
            << "Failed to canonicalize" << stdPathToQString(aff)
            << "error:" << QUtf8StringView(ec.message());
        return nullptr;
    }
    auto dicCanonical = std::filesystem::weakly_canonical(dic, ec);
    if (ec)
    {
        qCInfo(chatterinoSpellcheck)
            << "Failed to canonicalize" << stdPathToQString(dic)
            << "error:" << QUtf8StringView(ec.message());
        return nullptr;
    }

    return std::unique_ptr<SpellCheckerPrivate>{new SpellCheckerPrivate(
        std::move(dictionaryDirectory), hunspellPath(affCanonical).c_str(),
        hunspellPath(dicCanonical).c_str())};
}

SpellCheckerPrivate::SpellCheckerPrivate(
    std::unique_ptr<QTemporaryDir> dictionaryDirectory, const char *affpath,
    const char *dpath)
    : dictionaryDirectory_(std::move(dictionaryDirectory))
    , hunspell(affpath, dpath)
{
}

SpellChecker::SpellChecker()
    : private_(SpellCheckerPrivate::tryLoad(
          getSettings()->spellCheckingDefaultDictionary))
{
}
#else
class SpellCheckerPrivate
{
};
SpellChecker::SpellChecker() = default;
#endif

SpellChecker::~SpellChecker() = default;

bool SpellChecker::isLoaded() const
{
    return this->private_ != nullptr;
}

bool SpellChecker::check(const QString &word)
{
#ifdef CHATTERINO_WITH_SPELLCHECK
    if (!this->private_)
    {
        return true;
    }

    return this->private_->hunspell.spell(word.toStdString());
#else
    (void)word;
    return true;
#endif
}

std::vector<std::string> SpellChecker::suggestions(const QString &word)
{
#ifdef CHATTERINO_WITH_SPELLCHECK
    if (!this->private_)
    {
        return {};
    }

    auto stdWord = word.toStdString();
    if (this->private_->hunspell.spell(stdWord))
    {
        return {};
    }

    return this->private_->hunspell.suggest(stdWord);
#else
    (void)word;
    return {};
#endif
}

std::vector<DictionaryInfo> SpellChecker::getAvailableDictionaries() const
{
#ifdef CHATTERINO_WITH_SPELLCHECK
    std::vector<std::pair<QString, bool>> searchDirectories{
        {getApp()->getPaths().dictionariesDirectory, false},
    };

#    if defined(Q_OS_UNIX) and !defined(Q_OS_DARWIN)

    auto dataDirs = getXDGBaseDirectories(XDGDirectoryType::Data);
    for (const auto &dataDir : dataDirs)
    {
        searchDirectories.emplace_back(combinePath(dataDir, "hunspell"), true);
        searchDirectories.emplace_back(combinePath(dataDir, "myspell"), true);
        searchDirectories.emplace_back(combinePath(dataDir, "myspell/dicts"),
                                       true);
    }
#    endif

    std::vector<DictionaryInfo> dictionaries;

    for (const auto &[searchDirectory, isSystem] : searchDirectories)
    {
        qCDebug(chatterinoSpellcheck)
            << "Looking for dictionaries in" << searchDirectory
            << "isSystem:" << isSystem;
        for (const auto &dict :
             loadDictionariesFromDirectory(searchDirectory, isSystem))
        {
            if (dict.isSymbolicLink && dict.isSystem)
            {
                continue;
            }
            auto name = [&] -> QString {
                if (dict.isSystem)
                {
                    return dict.name % " (System)";
                }
                return dict.name;
            }();

            dictionaries.push_back(DictionaryInfo{
                .name = name,
                .path = dict.path,
                .isSymbolicLink = dict.isSymbolicLink,
                .isSystem = dict.isSystem,
            });
        }
    }

    std::ranges::sort(dictionaries,
                      [](const DictionaryInfo &lhs, const DictionaryInfo &rhs) {
                          return std::tie(lhs.isSystem, lhs.name, lhs.path) <
                                 std::tie(rhs.isSystem, rhs.name, rhs.path);
                      });

    dictionaries.insert(dictionaries.begin(),
                        DictionaryInfo{
                            .name = "English (US) (built-in)",
                            .path = QLatin1String(BUILT_IN_DICTIONARY),
                            .isSymbolicLink = false,
                            .isSystem = false,
                        });
    return dictionaries;
#else
    return {};
#endif
}

}  // namespace chatterino
