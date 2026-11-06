// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/trains/SatorinoTrainProducer.hpp"
#include "util/SatorinoGames.hpp"
#include "util/SatorinoTrain.hpp"
#include "util/SatorinoTrainSettings.hpp"
#include "Test.hpp"

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <pajlada/settings/setting.hpp>
#include <pajlada/settings/settingmanager.hpp>
#include <rapidjson/document.h>

#include <limits>
#include <memory>
#include <set>

using namespace chatterino::satorino;
namespace {
TrainUpdate update(TrainEventKind kind = TrainEventKind::Start,
                   qint64 at = 1000, std::optional<qint64> expires = 301000,
                   int level = 1)
{
    return {"train-1", kind, TrainType::Regular, level, at, expires};
}
TrainState running(TrainType type = TrainType::Regular)
{
    return {"train-1", TrainPhase::Running, type, 1, 1000, 301000};
}
}  // namespace
TEST(SatorinoTrain, NoDataProducerCannotInventARunningTrain)
{
    EXPECT_FALSE(trainProducer().available());
    EXPECT_FALSE(trainProducer().snapshot("100", 2000));
    EXPECT_EQ(trainDataStatus(true, trainProducer().available()),
              "Train data is not available.");
    EXPECT_EQ(trainDataStatus(false, false), "Train indicators: off");
    EXPECT_TRUE(trainDataStatus(true, true).contains("waiting"));
    TrainTracker tracker;
    EXPECT_EQ(tracker.state("100").phase, TrainPhase::NoData);
    EXPECT_FALSE(tracker.nextExpiry());
}
TEST(SatorinoTrain, StartProgressLevelUpAndEnd)
{
    TrainTracker tracker;
    ASSERT_TRUE(tracker.observe("100", update(), 1000));
    EXPECT_TRUE(tracker.state("100").running(1001));
    EXPECT_EQ(tracker.state("100").level, 1);
    ASSERT_TRUE(
        tracker.observe("100", update(TrainEventKind::Progress, 2000), 2000));
    ASSERT_TRUE(tracker.observe(
        "100", update(TrainEventKind::LevelUp, 3000, 303000, 2), 3000));
    EXPECT_EQ(tracker.state("100").level, 2);
    EXPECT_EQ(tracker.nextExpiry(), 303000);
    ASSERT_TRUE(
        tracker.observe("100", update(TrainEventKind::End, 4000, {}, 2), 4000));
    EXPECT_EQ(tracker.state("100").phase, TrainPhase::Ended);
    EXPECT_FALSE(tracker.state("100").running(4000));
    EXPECT_FALSE(tracker.nextExpiry());
    EXPECT_FALSE(tracker.observe(
        "100", update(TrainEventKind::Progress, 5000, 305000, 2), 5000));
}
TEST(SatorinoTrain, MissedEndExpiresExactlyAtDeadlineAndCannotRevive)
{
    TrainTracker tracker;
    ASSERT_TRUE(tracker.observe("100", update(), 1000));
    EXPECT_TRUE(tracker.expire(300999).isEmpty());
    EXPECT_TRUE(tracker.state("100").running(300999));
    EXPECT_FALSE(tracker.state("100").running(
        301000));  // Query also guards a delayed expiry timer.
    EXPECT_EQ(tracker.expire(301000), QStringList{"100"});
    EXPECT_EQ(tracker.state("100").phase, TrainPhase::Expired);
    EXPECT_TRUE(tracker.expire(301001).isEmpty());
    EXPECT_FALSE(tracker.observe(
        "100", update(TrainEventKind::Progress, 302000, 602000), 302000));
}
TEST(SatorinoTrain, MissingExpiryDoesNotSlideWithEveryContribution)
{
    TrainTracker tracker;
    ASSERT_TRUE(
        tracker.observe("100", update(TrainEventKind::Start, 1000, {}), 1000));
    EXPECT_EQ(tracker.nextExpiry(), 1000 + TRAIN_FALLBACK_MS);
    ASSERT_TRUE(tracker.observe(
        "100", update(TrainEventKind::Progress, 2000, {}), 2000));
    EXPECT_EQ(tracker.nextExpiry(), 1000 + TRAIN_FALLBACK_MS);
    ASSERT_TRUE(tracker.observe(
        "100", update(TrainEventKind::LevelUp, 3000, {}, 2), 3000));
    EXPECT_EQ(tracker.nextExpiry(), 3000 + TRAIN_FALLBACK_MS);
}
TEST(SatorinoTrain, PayloadDeadlineIsCappedAndPastDeadlineIsNotActive)
{
    TrainTracker tracker;
    ASSERT_TRUE(tracker.observe(
        "100",
        update(TrainEventKind::Start, 1000, std::numeric_limits<qint64>::max()),
        1000));
    EXPECT_EQ(tracker.nextExpiry(), 1000 + TRAIN_FALLBACK_MS);
    ASSERT_TRUE(
        tracker.observe("200", update(TrainEventKind::Start, 1000, 999), 1000));
    EXPECT_EQ(tracker.state("200").phase, TrainPhase::Expired);
}
TEST(SatorinoTrain, StaleUpdatesWrongIdsAndLevelRegressionAreRejected)
{
    TrainTracker tracker;
    ASSERT_TRUE(tracker.observe(
        "100", update(TrainEventKind::Start, 3000, 303000, 2), 3000));
    EXPECT_FALSE(
        tracker.observe("100", update(TrainEventKind::Progress, 2000), 3000));
    EXPECT_FALSE(
        tracker.observe("100", update(TrainEventKind::Progress, 4000), 4000));
    auto next = update(TrainEventKind::Progress, 4000, 304000, 3);
    next.id = "different-train";
    EXPECT_FALSE(tracker.observe("100", next, 4000));
    next.kind = TrainEventKind::Start;
    ASSERT_TRUE(tracker.observe("100", next, 4000));
    EXPECT_EQ(tracker.state("100").id, next.id);
    EXPECT_FALSE(
        tracker.observe("100", update(TrainEventKind::End, 3500), 4000));
}
TEST(SatorinoTrain, InvalidIdentityTimeAndLevelNeverAllocateState)
{
    TrainTracker tracker;
    EXPECT_FALSE(tracker.observe("", update(), 1000));
    EXPECT_FALSE(tracker.observe("bad", update(), 1000));
    auto invalid = update();
    invalid.id.clear();
    EXPECT_FALSE(tracker.observe("100", invalid, 1000));
    EXPECT_FALSE(tracker.observe(
        "100", update(TrainEventKind::Start, 1000, 2000, 0), 1000));
    EXPECT_FALSE(tracker.observe("100", update(), 999));
    EXPECT_FALSE(
        tracker.observe("100", update(), std::numeric_limits<qint64>::max()));
    EXPECT_EQ(tracker.size(), 0U);
}
TEST(SatorinoTrain, StateIsBoundedAndPrunedOnCloseAndDisable)
{
    TrainTracker tracker;
    for (std::size_t i = 0; i < MAX_TRAIN_CHANNELS; ++i)
        ASSERT_TRUE(tracker.observe(
            QString::number(static_cast<qulonglong>(i + 1)), update(), 1000));
    EXPECT_FALSE(tracker.observe("99999", update(), 1000));
    tracker.retain({"1"});
    EXPECT_EQ(tracker.size(), 1U);
    tracker.setEnabled(false);
    EXPECT_EQ(tracker.size(), 0U);
    EXPECT_FALSE(tracker.observe("1", update(), 1000));
    EXPECT_TRUE(tracker.expire(999999).isEmpty());
    tracker.setEnabled(true);
    EXPECT_TRUE(tracker.observe("1", update(), 1000));
}
TEST(SatorinoTrain, TypesAreExplicitAndGoldenIsNeverInferredFromEmotes)
{
    EXPECT_EQ(trainType("regular"), TrainType::Regular);
    EXPECT_EQ(trainType("golden_kappa", true), TrainType::GoldenKappa);
    EXPECT_EQ(trainType("treasure", true), TrainType::Treasure);
    EXPECT_EQ(trainType("regular", true), TrainType::Shared);
    EXPECT_EQ(trainType(""), TrainType::Other);
    EXPECT_EQ(trainType("Kappa"), TrainType::Other);
    EXPECT_EQ(trainType("new_twitch_type", true), TrainType::Other);
    for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
        EXPECT_EQ(trainType(trainTypeKey(static_cast<TrainType>(i))),
                  static_cast<TrainType>(i));
}
TEST(SatorinoTrain, DefaultsAreEnabledDistinctAndGoldenIsGold)
{
    EXPECT_TRUE(TRAIN_ENABLED_DEFAULT);
    const auto colours = defaultTrainColours();
    EXPECT_NE(colours.values[0],
              QColor("#ff0000"));  // Stock dark-theme live dot.
    GameList games;
    for (int i = 0; i < 8; ++i)
    {
        const auto gameColour = nextGameColour(games);
        EXPECT_NE(colours.values[0], gameColour);
        games.push_back({QString::number(i), {}, {}, gameColour});
    }
    std::set<QRgb> unique;
    for (const auto &colour : colours.values)
    {
        EXPECT_TRUE(colour.isValid());
        EXPECT_TRUE(unique.insert(colour.rgba()).second);
    }
    EXPECT_EQ(colours.values[static_cast<std::size_t>(TrainType::GoldenKappa)],
              QColor("#ffd23f"));
}
TEST(SatorinoTrain, TrainBeatsGameBeatsStockAndGameStarIsIndependent)
{
    const auto colours = defaultTrainColours();
    const auto game = QColor("#173f5f");
    for (bool gameStar : {false, true})
        for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
        {
            const auto type = static_cast<TrainType>(i);
            const auto style = highlightedLiveIndicator(
                true, game, gameStar, true, {running(type)}, colours, 2000);
            ASSERT_TRUE(style);
            EXPECT_EQ(style->kind, LiveIndicator::Train);
            EXPECT_EQ(style->colour, colours.values[i]);
            EXPECT_EQ(style->shape, type == TrainType::GoldenKappa
                                        ? IndicatorShape::Star
                                        : IndicatorShape::Circle);
        }
    const auto gameStyle = highlightedLiveIndicator(true, game, true, false,
                                                    {running()}, colours, 2000);
    ASSERT_TRUE(gameStyle);
    EXPECT_EQ(gameStyle->kind, LiveIndicator::Game);
    EXPECT_EQ(gameStyle->colour, game);
    EXPECT_EQ(gameStyle->shape, IndicatorShape::Star);
    EXPECT_FALSE(highlightedLiveIndicator(true, {}, false, false, {running()},
                                          colours, 2000));
    EXPECT_FALSE(highlightedLiveIndicator(false, game, true, true, {running()},
                                          colours, 2000));
    EXPECT_EQ(liveIndicator(true, false), LiveIndicator::Dot);
}
TEST(SatorinoTrain, GoldenWinsAcrossAggregateChildrenAndExpiryFallsBackToGame)
{
    auto colours = defaultTrainColours();
    const QColor exact(11, 37, 81, 65);
    colours.values[static_cast<std::size_t>(TrainType::GoldenKappa)] = exact;
    const auto style =
        highlightedLiveIndicator(true, QColor("red"), false, true,
                                 {running(), running(TrainType::Treasure),
                                  running(TrainType::GoldenKappa)},
                                 colours, 2000);
    ASSERT_TRUE(style);
    EXPECT_EQ(style->colour,
              exact);  // RGB and alpha stay exact, with no state shading.
    EXPECT_EQ(style->shape, IndicatorShape::Star);
    const auto expired = highlightedLiveIndicator(
        true, QColor("red"), false, true, {running()}, colours, 301000);
    ASSERT_TRUE(expired);
    EXPECT_EQ(expired->kind, LiveIndicator::Game);
    EXPECT_EQ(expired->shape, IndicatorShape::Circle);
}
TEST(SatorinoTrain, PaletteRoundTripsEveryTypeAndAlphaWithoutShading)
{
    auto colours = defaultTrainColours();
    for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
        colours.values[i].setAlpha(static_cast<int>(i) * 30);
    EXPECT_EQ(trainColoursFromJson(trainColoursToJson(colours)), colours);
    rapidjson::Document doc;
    const auto value =
        pajlada::Serialize<TrainColours>::get(colours, doc.GetAllocator());
    EXPECT_EQ(pajlada::Deserialize<TrainColours>::get(value), colours);
    const auto partial =
        trainColoursFromJson(QJsonObject{{"regular", "#123456"},
                                         {"golden_kappa", "invalid"},
                                         {"future", "#ffffff"}});
    EXPECT_EQ(partial.values[0], QColor("#123456"));
    EXPECT_EQ(partial.values[1], defaultTrainColours().values[1]);
    doc.Parse("null");
    bool error = false;
    EXPECT_EQ(pajlada::Deserialize<TrainColours>::get(doc, &error),
              defaultTrainColours());
    EXPECT_TRUE(error);
}
TEST(SatorinoTrain,
     RegisteredSettingsPersistPaletteAlphaAndDisableIndependentlyOfGameStar)
{
    auto manager = std::make_shared<pajlada::Settings::SettingManager>();
    manager->saveMethod =
        pajlada::Settings::SettingManager::SaveMethod::SaveManually;
    const pajlada::Settings::SettingOption option =
        pajlada::Settings::SettingOption::CompareBeforeSet;
    pajlada::Settings::Setting<bool> enabled(
        TRAIN_ENABLED_PATH, TRAIN_ENABLED_DEFAULT, option, manager);
    pajlada::Settings::Setting<TrainColours> stored(
        TRAIN_COLOURS_PATH, defaultTrainColours(), option, manager);
    pajlada::Settings::Setting<bool> gameStar("/satorino/live/gameStar", true,
                                              option, manager);
    EXPECT_TRUE(enabled.getValue());
    EXPECT_EQ(stored.getValue(), defaultTrainColours());
    auto chosen = stored.getValue();
    chosen.values[1] = QColor(10, 20, 30, 47);
    stored.setValue(chosen);
    enabled.setValue(false);
    pajlada::Settings::Setting<TrainColours> reloaded(
        TRAIN_COLOURS_PATH, defaultTrainColours(), option, manager);
    pajlada::Settings::Setting<bool> disabled(
        TRAIN_ENABLED_PATH, TRAIN_ENABLED_DEFAULT, option, manager);
    EXPECT_EQ(reloaded.getValue(), chosen);
    EXPECT_FALSE(disabled.getValue());
    EXPECT_TRUE(gameStar.getValue());
    ASSERT_TRUE(manager->get(TRAIN_COLOURS_PATH)->IsObject());
    enabled.setValue(true);
    EXPECT_EQ(reloaded.getValue(), chosen);
}
