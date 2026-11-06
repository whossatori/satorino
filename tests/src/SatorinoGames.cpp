// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoGames.hpp"
#include "util/SatorinoGamesSettings.hpp"

#include "Test.hpp"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <rapidjson/document.h>
#include <pajlada/settings/setting.hpp>
#include <pajlada/settings/settingmanager.hpp>

#include <memory>

#include <set>

using namespace chatterino::satorino;
namespace {
HighlightedGame game(const QString &id = "1", const QString &name = "Minecraft")
{
    return {id, name,
            "https://static-cdn.jtvnw.net/boxart/1-{width}x{height}.jpg",
            QColor("#54aaff")};
}
}  // namespace
TEST(SatorinoGames, JsonRoundTripPreservesOrderAndFields)
{
    const GameList input{game(), game("2", "Just Chatting")};
    EXPECT_EQ(gamesFromJson(gamesToJson(input)), input);
    EXPECT_EQ(gamesFromJsonText(QString::fromUtf8(
                  QJsonDocument(gamesToJson(input)).toJson())),
              input);
}
TEST(SatorinoGames, MalformedAndMissingTopLevelValuesAreEmpty)
{
    for (const auto &value : {QString{}, QString("garbage"), QString("null"),
                              QString("{}"), QString("42"), QString("[]")})
        EXPECT_TRUE(gamesFromJsonText(value).isEmpty());
    EXPECT_TRUE(gamesFromJson(QJsonArray{QJsonObject{}, 42, "bad",
                                         QJsonObject{{"id", "abc"}}})
                    .isEmpty());
}
TEST(SatorinoGames, MissingOptionalFieldsUseSafeDefaults)
{
    const auto result = gamesFromJson(
        QJsonArray{QJsonObject{{"name", " Just Chatting "}},
                   QJsonObject{{"id", "123"},
                               {"colour", "garbage"},
                               {"boxArtUrl", "http://invalid.test/art"}}});
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].name, "Just Chatting");
    EXPECT_TRUE(result[0].colour.isValid());
    EXPECT_TRUE(result[1].name.isEmpty());
    EXPECT_TRUE(result[1].boxArtUrl.isEmpty());
    EXPECT_NE(result[0].colour, result[1].colour);
}
TEST(SatorinoGames, RegisteredSettingAdapterStoresAnActualArray)
{
    rapidjson::Document doc;
    const GameList input{game()};
    const auto encoded =
        pajlada::Serialize<GameList>::get(input, doc.GetAllocator());
    ASSERT_TRUE(encoded.IsArray());
    ASSERT_EQ(encoded.Size(), 1U);
    EXPECT_TRUE(encoded[0]["colour"].IsString());
    EXPECT_EQ(pajlada::Deserialize<GameList>::get(encoded), input);
    doc.Parse("{\"not\":\"a list\"}");
    bool error = false;
    EXPECT_TRUE(pajlada::Deserialize<GameList>::get(doc, &error).isEmpty());
    EXPECT_TRUE(error);
}
TEST(SatorinoGames, SettingAdapterSkipsGarbageEntries)
{
    rapidjson::Document doc;
    doc.Parse(
        "[null,42,{}, {\"name\":\"Game\",\"colour\":false}, {\"id\":\"bad\"}]");
    const auto result = pajlada::Deserialize<GameList>::get(doc);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "Game");
    EXPECT_TRUE(result[0].colour.isValid());
}
TEST(SatorinoGames, MigrationUsesNamesNumericIdsAndSkipsBlankLines)
{
    const auto result = migrateGames(
        false, {}, " Just Chatting \n\n 509658 \r\nMinecraft\njust chatting\n");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].name, "Just Chatting");
    EXPECT_TRUE(result[0].id.isEmpty());
    EXPECT_EQ(result[1].id, "509658");
    EXPECT_TRUE(result[1].name.isEmpty());
    EXPECT_EQ(result[2].name, "Minecraft");
    EXPECT_NE(result[0].colour, result[1].colour);
}
TEST(SatorinoGames, ExplicitEmptyV2NeverResurrectsLegacyLines)
{
    EXPECT_TRUE(migrateGames(true, {}, "Just Chatting\n123").isEmpty());
    const auto list = GameList{game()};
    EXPECT_EQ(migrateGames(true, list, "Other"), list);
    EXPECT_TRUE(migrateGames(false, {}, " \n\t").isEmpty());
}
TEST(SatorinoGames, IdTakesPrecedenceAndNameIsTheFallback)
{
    const auto g = game("123", "Old Name");
    EXPECT_TRUE(gameMatches(g, "New Name", "123"));
    EXPECT_FALSE(gameMatches(g, "Old Name", "456"));
    EXPECT_TRUE(gameMatches(g, "old name", ""));
    EXPECT_TRUE(gameMatches(game("", "Old Name"), " OLD NAME ", "456"));
    EXPECT_FALSE(gameMatches(game("123", ""), "123", ""));
}
TEST(SatorinoGames, NamesUseNfcCaseFoldAndNeverPartialMatching)
{
    const auto g = game("", QString::fromUtf8("POKÉMON"));
    EXPECT_TRUE(gameMatches(g, QString::fromUtf8("Poke\xCC\x81mon")));
    EXPECT_FALSE(gameMatches(g, QString::fromUtf8("Pokémon Legends")));
    EXPECT_FALSE(gameMatches(game("", "Minecraft"), "Mine"));
    EXPECT_FALSE(gameMatches(game("", ""), ""));
}
TEST(SatorinoGames, FirstUserListMatchWinsAcrossSplitsAndAggregates)
{
    const GameList games{game("1", "First"), game("2", "Second")};
    EXPECT_EQ(firstMatchingGame(
                  games, QVector<LiveGame>{{"2", "Second"}, {"1", "First"}}),
              0);
    EXPECT_EQ(firstMatchingGame(games, QVector<LiveGame>{{"2", "Second"}}), 1);
    EXPECT_FALSE(firstMatchingGame(games, QVector<LiveGame>{{"9", "Other"}}));
}
TEST(SatorinoGames, DedupeKeepsFirstIdOrCanonicalNameAndCapsCount)
{
    QJsonArray input;
    input.push_back(
        QJsonObject{{"id", "1"}, {"name", "First"}, {"colour", "#123456"}});
    input.push_back(QJsonObject{{"id", "1"}, {"name", "Renamed"}});
    input.push_back(QJsonObject{{"name", " first "}});
    for (int i = 2; i < 100; ++i)
        input.push_back(QJsonObject{{"id", QString::number(i)},
                                    {"name", "Game " + QString::number(i)}});
    const auto result = gamesFromJson(input);
    ASSERT_EQ(result.size(), MAX_HIGHLIGHTED_GAMES);
    EXPECT_EQ(result[0].name, "First");
    EXPECT_EQ(result[0].colour, QColor("#123456"));
    EXPECT_EQ(result.back().id, "50");
}
TEST(SatorinoGames, DefaultsAreDistinctOpaqueAndAvoidStockRed)
{
    GameList games;
    std::set<QRgb> colours;
    for (qsizetype i = 0; i < MAX_HIGHLIGHTED_GAMES; ++i)
    {
        const auto colour = nextGameColour(games);
        EXPECT_EQ(colour.alpha(), 255);
        EXPECT_NE(colour, QColor("#ff0000"));
        EXPECT_TRUE(colours.insert(colour.rgba()).second);
        games.push_back({QString::number(i), {}, {}, colour});
    }
}
TEST(SatorinoGames, SeenMetadataEnrichesMigrationWithoutChangingColourOrOrder)
{
    auto games = migrateGames(false, {}, "123\nMinecraft");
    const auto colour = games[0].colour;
    EXPECT_TRUE(learnGame(games, "123", "Just Chatting"));
    EXPECT_EQ(games[0].name, "Just Chatting");
    EXPECT_EQ(games[0].colour, colour);
    EXPECT_TRUE(learnGame(games, "456", "Minecraft", game().boxArtUrl));
    EXPECT_EQ(games[1].id, "456");
    EXPECT_FALSE(games[1].boxArtUrl.isEmpty());
    EXPECT_FALSE(learnGame(games, "123", "Just Chatting"));
    EXPECT_FALSE(learnGame(games, "999", "Unselected"));
}
TEST(SatorinoGames, PriorityResolvesStockAndGameColours)
{
    IndicatorColours colours{QColor("red"), QColor("cyan")};
    EXPECT_FALSE(indicatorColour(liveIndicator(false, true), colours));
    EXPECT_EQ(indicatorColour(liveIndicator(true, false), colours),
              colours.stock);
    EXPECT_EQ(indicatorColour(liveIndicator(true, true), colours),
              colours.game);
    EXPECT_EQ(indicatorColour(LiveIndicator::Game,
                              IndicatorColours{QColor("red"), {}}),
              QColor("red"));
}
TEST(SatorinoGames, OutlineContrastsWithBrightAndDarkFills)
{
    EXPECT_EQ(dotOutline(QColor("white")), QColor("black"));
    EXPECT_EQ(dotOutline(QColor("black")), QColor("white"));
}
TEST(SatorinoGames, BoxArtReplacesDimensionsAndRejectsInvalidUrlsOrSizes)
{
    EXPECT_EQ(gameBoxArtUrl(game().boxArtUrl),
              "https://static-cdn.jtvnw.net/boxart/1-52x72.jpg");
    EXPECT_EQ(gameBoxArtUrl(game().boxArtUrl, 26, 36),
              "https://static-cdn.jtvnw.net/boxart/1-26x36.jpg");
    EXPECT_TRUE(gameBoxArtUrl("http://example.invalid/art").isEmpty());
    EXPECT_TRUE(gameBoxArtUrl("garbage").isEmpty());
    EXPECT_TRUE(
        gameBoxArtUrl("https://user:pass@example.invalid/art").isEmpty());
    EXPECT_TRUE(gameBoxArtUrl(game().boxArtUrl, 0, 72).isEmpty());
    EXPECT_TRUE(gameBoxArtUrl(game().boxArtUrl, 52, 1000).isEmpty());
}
TEST(SatorinoGames, SearchRequiresTwoCharactersAndThreeHundredMs)
{
    GameSearchState state;
    state.edit(" a ", 0);
    EXPECT_FALSE(state.longEnough());
    EXPECT_FALSE(state.takeReady(1000));
    state.edit(" ab ", 1000);
    EXPECT_TRUE(state.longEnough());
    EXPECT_FALSE(state.takeReady(1299));
    const auto request = state.takeReady(1300);
    ASSERT_TRUE(request);
    EXPECT_EQ(request->query, "ab");
    EXPECT_TRUE(state.accepts(request->generation));
    EXPECT_FALSE(state.takeReady(2000));
}
TEST(SatorinoGames,
     NewEditsRestartDebounceAndIgnoreStaleResultsIncludingSameQuery)
{
    GameSearchState state;
    state.edit("Minecraft", 0);
    const auto old = state.takeReady(300);
    ASSERT_TRUE(old);
    state.edit("Other", 350);
    EXPECT_FALSE(state.accepts(old->generation));
    state.edit("Minecraft", 400);
    EXPECT_FALSE(state.takeReady(699));
    const auto current = state.takeReady(700);
    ASSERT_TRUE(current);
    EXPECT_FALSE(state.accepts(old->generation));
    EXPECT_TRUE(state.accepts(current->generation));
    state.edit("", 800);
    EXPECT_FALSE(state.accepts(current->generation));
    EXPECT_FALSE(state.takeReady(2000));
}

TEST(SatorinoGames, ResolvedMigrationAliasesKeepFirstColourAndOrder)
{
    auto games = migrateGames(false, {}, "Just Chatting\n123");
    ASSERT_EQ(games.size(), 2);
    const auto colour = games.front().colour;
    ASSERT_TRUE(learnGame(games, "123", "Just Chatting"));
    ASSERT_EQ(games.size(), 1);
    EXPECT_EQ(games.front().id, "123");
    EXPECT_EQ(games.front().colour, colour);
}
TEST(SatorinoGames, PersistedEmptyOrMalformedV2CountsAsPresent)
{
    auto manager = std::make_shared<pajlada::Settings::SettingManager>();
    manager->saveMethod =
        pajlada::Settings::SettingManager::SaveMethod::SaveManually;
    const char *path = "/satorino/live/gamesV2";
    pajlada::Settings::Setting<GameList> stored(
        path, GameList{}, pajlada::Settings::SettingOption::CompareBeforeSet,
        manager);
    EXPECT_FALSE(stored.hasValueBeenSet());
    stored.setValue(migrateGames(stored.hasValueBeenSet(), stored.getValue(),
                                 "Just Chatting"));
    ASSERT_TRUE(stored.hasValueBeenSet());
    ASSERT_EQ(stored.getValue().size(), 1);
    stored.setValue({});
    pajlada::Settings::Setting<GameList> reloaded(
        path, GameList{}, pajlada::Settings::SettingOption::CompareBeforeSet,
        manager);
    ASSERT_TRUE(reloaded.hasValueBeenSet());
    EXPECT_TRUE(migrateGames(reloaded.hasValueBeenSet(), reloaded.getValue(),
                             "Just Chatting")
                    .isEmpty());
    ASSERT_TRUE(manager->get(path)->IsArray());
    EXPECT_EQ(manager->get(path)->Size(), 0U);
    manager->set(path, rapidjson::Value(rapidjson::kNullType));
    EXPECT_TRUE(reloaded.hasValueBeenSet());
    EXPECT_TRUE(migrateGames(reloaded.hasValueBeenSet(), reloaded.getValue(),
                             "Just Chatting")
                    .isEmpty());
}

TEST(SatorinoGames, AddResetCancelsPendingDebounce)
{
    GameSearchState state;
    state.edit("Minecraft", 0);
    state.resetAfterAdd(100);
    EXPECT_FALSE(state.longEnough());
    EXPECT_FALSE(state.takeReady(1000));
    state.edit("Other", 1100);
    EXPECT_FALSE(state.takeReady(1399));
    ASSERT_TRUE(state.takeReady(1400));
}
TEST(SatorinoGames, AddResetRejectsLateSuccessOrErrorFromOlderGeneration)
{
    GameSearchState state;
    state.edit("Minecraft", 0);
    const auto old = state.takeReady(300);
    ASSERT_TRUE(old);
    state.resetAfterAdd(350);
    EXPECT_FALSE(state.accepts(old->generation));
    EXPECT_FALSE(state.takeReady(1000));
    state.edit("Minecraft", 1100);
    const auto next = state.takeReady(1400);
    ASSERT_TRUE(next);
    EXPECT_TRUE(state.accepts(next->generation));
    EXPECT_FALSE(state.accepts(old->generation));
}

TEST(SatorinoGames, ColourAlphaRoundTripsWithoutShading)
{
    const QColor chosen(17, 34, 51, 87);
    const GameList games{{"123", "Game", {}, chosen}};
    const auto result = gamesFromJson(gamesToJson(games));
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().colour, chosen);
}
