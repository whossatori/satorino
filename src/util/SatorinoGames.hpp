// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoLive.hpp"

#include <QColor>
#include <QJsonArray>
#include <QString>
#include <QVector>

#include <optional>

namespace chatterino::satorino {

inline constexpr qsizetype MAX_HIGHLIGHTED_GAMES = 50;
struct HighlightedGame {
    QString id;
    QString name;
    QString boxArtUrl;
    QColor colour;
    bool operator==(const HighlightedGame &) const = default;
};
using GameList = QVector<HighlightedGame>;
struct LiveGame {
    QString id;
    QString name;
};
GameList gamesFromJson(const QJsonArray &json);
GameList gamesFromJsonText(const QString &json);
QJsonArray gamesToJson(const GameList &games);
GameList migrateGames(bool v2Present, const GameList &current,
                      const QString &legacy);
QColor nextGameColour(const GameList &games);
bool gameMatches(const HighlightedGame &game, const QString &name,
                 const QString &id = {});
std::optional<qsizetype> firstMatchingGame(const GameList &games,
                                           const QVector<LiveGame> &live);
bool learnGame(GameList &games, const QString &id, const QString &name,
               const QString &boxArtUrl = {});
QString gameBoxArtUrl(QString url, int width = 52, int height = 72);

struct IndicatorColours {
    QColor stock;
    std::optional<QColor> game;
};
std::optional<QColor> indicatorColour(LiveIndicator kind,
                                      const IndicatorColours &colours);
QColor dotOutline(const QColor &fill);

struct GameSearchRequest {
    quint64 generation;
    QString query;
};
// Time is injected so debounce, minimum length and stale responses are testable.
class GameSearchState
{
public:
    static constexpr int DEBOUNCE_MS = 300;
    void edit(const QString &query, qint64 nowMs);
    void resetAfterAdd(qint64 nowMs);
    std::optional<GameSearchRequest> takeReady(qint64 nowMs);
    bool accepts(quint64 generation) const;
    bool longEnough() const;

private:
    QString query_;
    quint64 generation_ = 0;
    qint64 due_ = 0;
    bool launched_ = false;
};
}  // namespace chatterino::satorino
