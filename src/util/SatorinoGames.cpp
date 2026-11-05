// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoGames.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cstddef>

namespace chatterino::satorino {
namespace {
QString canonical(const QString &s)
{
    return s.trimmed().normalized(QString::NormalizationForm_C).toCaseFolded();
}
bool numericId(const QString &s)
{
    return !s.isEmpty() && s.size() <= 32 &&
           std::all_of(s.begin(), s.end(), [](QChar c) {
               return c >= '0' && c <= '9';
           });
}
bool sameGame(const HighlightedGame &a, const HighlightedGame &b)
{
    if (!a.id.isEmpty() && !b.id.isEmpty())
        return a.id == b.id;
    return !a.name.isEmpty() && !b.name.isEmpty() &&
           canonical(a.name) == canonical(b.name);
}
}  // namespace

QColor nextGameColour(const GameList &games)
{
    // Eight hues away from stock red, then distinct shades for longer lists.
    static constexpr std::array<int, 8> hues{190, 45,  270, 95,
                                             325, 215, 25,  160};
    for (int i = 0; i < 64; ++i)
    {
        const QColor colour =
            QColor::fromHsl(hues[static_cast<std::size_t>(i % 8)],
                            215 - (i / 8) * 12, 135 + (i / 8) * 8);
        if (std::none_of(games.begin(), games.end(), [&colour](const auto &g) {
                return g.colour == colour;
            }))
            return colour;
    }
    return QColor("#26c6da");
}
GameList gamesFromJson(const QJsonArray &json)
{
    GameList result;
    for (const auto &value : json)
    {
        if (!value.isObject())
            continue;
        const auto obj = value.toObject();
        HighlightedGame game{obj.value("id").toString().trimmed(),
                             obj.value("name").toString().trimmed(),
                             obj.value("boxArtUrl").toString(),
                             QColor(obj.value("colour").toString())};
        if ((!game.id.isEmpty() && !numericId(game.id)) ||
            game.name.size() > 200 ||
            (game.id.isEmpty() && game.name.isEmpty()))
            continue;
        if (gameBoxArtUrl(game.boxArtUrl).isEmpty())
            game.boxArtUrl.clear();
        if (!game.colour.isValid() || game.colour.alpha() != 255)
            game.colour = nextGameColour(result);
        if (std::any_of(result.begin(), result.end(),
                        [&game](const auto &other) {
                            return sameGame(game, other);
                        }))
            continue;
        result.push_back(game);
        if (result.size() >= MAX_HIGHLIGHTED_GAMES)
            break;
    }
    return result;
}
GameList gamesFromJsonText(const QString &json)
{
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    return doc.isArray() ? gamesFromJson(doc.array()) : GameList{};
}
QJsonArray gamesToJson(const GameList &games)
{
    QJsonArray result;
    for (const auto &game : games)
        result.push_back(
            QJsonObject{{"id", game.id},
                        {"name", game.name},
                        {"boxArtUrl", game.boxArtUrl},
                        {"colour", game.colour.name(QColor::HexRgb)}});
    return result;
}
GameList migrateGames(bool v2Present, const GameList &current,
                      const QString &legacy)
{
    if (v2Present)
        return current;
    QJsonArray items;
    for (const auto &line : legacy.split('\n'))
    {
        const auto value = line.trimmed();
        if (!value.isEmpty())
            items.push_back(
                QJsonObject{{"id", numericId(value) ? value : QString{}},
                            {"name", numericId(value) ? QString{} : value}});
    }
    return gamesFromJson(items);
}
bool gameMatches(const HighlightedGame &game, const QString &name,
                 const QString &id)
{
    if (!game.id.isEmpty() && !id.trimmed().isEmpty())
        return game.id == id.trimmed();
    return !game.name.isEmpty() && !name.trimmed().isEmpty() &&
           canonical(game.name) == canonical(name);
}
std::optional<qsizetype> firstMatchingGame(const GameList &games,
                                           const QVector<LiveGame> &live)
{
    // User list order wins, regardless of split/aggregate iteration order.
    for (qsizetype i = 0; i < games.size(); ++i)
        for (const auto &stream : live)
            if (gameMatches(games[i], stream.name, stream.id))
                return i;
    return std::nullopt;
}
bool learnGame(GameList &games, const QString &id, const QString &name,
               const QString &boxArtUrl)
{
    bool changed = false;
    if (!numericId(id) || name.trimmed().isEmpty() || name.size() > 200)
        return false;
    for (auto &game : games)
    {
        if (!gameMatches(game, name, id))
            continue;
        if (game.id.isEmpty())
        {
            game.id = id;
            changed = true;
        }
        if (game.name.isEmpty())
        {
            game.name = name.trimmed();
            changed = true;
        }
        if (game.boxArtUrl.isEmpty() && !gameBoxArtUrl(boxArtUrl).isEmpty())
        {
            game.boxArtUrl = boxArtUrl;
            changed = true;
        }
    }
    // Migrated name and numeric-ID aliases can become the same known game.
    if (changed)
        games = gamesFromJson(gamesToJson(games));
    return changed;
}
QString gameBoxArtUrl(QString url, int width, int height)
{
    if (width < 1 || width > 256 || height < 1 || height > 256 ||
        url.size() > 4096)
        return {};
    url.replace("{width}", QString::number(width));
    url.replace("{height}", QString::number(height));
    const QUrl parsed(url, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.scheme() != "https" ||
        parsed.host().isEmpty() || !parsed.userInfo().isEmpty())
        return {};
    return parsed.toString();
}
std::optional<QColor> indicatorColour(LiveIndicator kind,
                                      const IndicatorColours &colours,
                                      TrainColourKind train)
{
    switch (kind)
    {
        case LiveIndicator::None:
            return std::nullopt;
        case LiveIndicator::Dot:
            return colours.stock;
        case LiveIndicator::Game:
            return colours.game.value_or(colours.stock);
        case LiveIndicator::Train: {
            const auto index = static_cast<std::size_t>(train);
            return index < colours.trains.size()
                       ? colours.trains[index].value_or(colours.stock)
                       : colours.stock;
        }
    }
    return colours.stock;
}
QColor dotOutline(const QColor &fill)
{
    // Relative luminance approximation: contrast against the user's fill.
    return (299 * fill.red() + 587 * fill.green() + 114 * fill.blue() >= 140000)
               ? QColor(Qt::black)
               : QColor(Qt::white);
}
void GameSearchState::edit(const QString &query, qint64 nowMs)
{
    this->query_ = query.trimmed();
    ++this->generation_;
    this->due_ = nowMs + DEBOUNCE_MS;
    this->launched_ = false;
}
void GameSearchState::resetAfterAdd(qint64 nowMs)
{
    this->edit({}, nowMs);
}

bool GameSearchState::longEnough() const
{
    return this->query_.size() >= 2;
}
std::optional<GameSearchRequest> GameSearchState::takeReady(qint64 nowMs)
{
    if (!this->longEnough() || this->launched_ || nowMs < this->due_)
        return std::nullopt;
    this->launched_ = true;
    return GameSearchRequest{this->generation_, this->query_};
}
bool GameSearchState::accepts(quint64 generation) const
{
    return this->longEnough() && generation == this->generation_;
}
}  // namespace chatterino::satorino
