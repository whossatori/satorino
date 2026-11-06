// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoFlags.hpp"
#include "util/SatorinoSuspiciousUsers.hpp"
#include "util/RapidJsonSerializeQString.hpp"
#include "Test.hpp"
#include <QColor>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <pajlada/settings/setting.hpp>
#include <pajlada/settings/settingmanager.hpp>
#include <initializer_list>
#include <memory>
#include <utility>
using namespace chatterino::satorino;
namespace {
UserFlag record(const QString &room = "100", const QString &user = "200",
                qint64 at = 1000)
{
    return {room, user, "channel", "user", at};
}
}  // namespace
TEST(SatorinoFlags, ToggleUsesRoomAndUserNotLabels)
{
    FlagStore store;
    ASSERT_TRUE(store.toggle(record()));
    EXPECT_TRUE(store.contains("100", "200"));
    EXPECT_FALSE(store.contains("101", "200"));
    EXPECT_FALSE(store.contains("100", "201"));
    ASSERT_TRUE(store.toggle(record("101")));
    auto renamed = record();
    renamed.channelLabel = "renamed";
    renamed.userLabel = "Renamed User";
    ASSERT_TRUE(store.toggle(renamed));
    EXPECT_FALSE(store.contains("100", "200"));
    EXPECT_TRUE(store.contains("101", "200"));
}
TEST(SatorinoFlags, BoundsNeverPreventRemovingAnExistingKey)
{
    FlagStore store;
    for (int i = 1; i <= MAX_FLAGS_PER_ROOM; ++i)
        ASSERT_TRUE(store.toggle(record("100", QString::number(i))));
    QString error;
    EXPECT_FALSE(store.toggle(record("100", "999"), &error));
    EXPECT_TRUE(error.contains("limit"));
    EXPECT_TRUE(store.toggle(record("100", "1")));
    EXPECT_TRUE(store.toggle(record("100", "999")));
    for (int room = 101; room <= 104; ++room)
        for (int user = 1; user <= MAX_FLAGS_PER_ROOM; ++user)
            ASSERT_TRUE(store.toggle(
                record(QString::number(room), QString::number(user))));
    EXPECT_EQ(store.size(), MAX_FLAGS);
    EXPECT_FALSE(store.toggle(record("105", "1")));
    EXPECT_TRUE(store.remove("101", "1"));
    EXPECT_TRUE(store.toggle(record("105", "1")));
}
TEST(SatorinoFlags, MalformedInputAndVersionsAreSafe)
{
    for (const auto &json :
         {QString{}, QString("garbage"), QString("[]"), QString("null"),
          QString(R"({"version":2,"entries":[]})"),
          QString(R"({"version":1,"entries":42})")})
        EXPECT_EQ(FlagStore::fromJson(json).size(), 0);
    FlagStore store;
    EXPECT_FALSE(store.toggle(record("0")));
    EXPECT_FALSE(store.toggle(record("01")));
    EXPECT_FALSE(store.toggle(record("kick:100")));
    EXPECT_FALSE(store.toggle(record("100", "bad")));
    EXPECT_FALSE(store.toggle(record("100", "200", 0)));
    EXPECT_FALSE(store.toggle(record("100", "200", 253402300800000LL)));
    EXPECT_EQ(store.size(), 0);
}
TEST(SatorinoFlags, VersionedJsonRoundTripSanitisesLabelsAndKeepsTimestamp)
{
    FlagStore store;
    auto f = record();
    f.channelLabel = " <channel> ";
    f.userLabel = "User\nName";
    ASSERT_TRUE(store.toggle(f));
    const auto json = store.toJson();
    EXPECT_EQ(QJsonDocument::fromJson(json.toUtf8())
                  .object()
                  .value("version")
                  .toInt(),
              1);
    const auto loaded = FlagStore::fromJson(json);
    EXPECT_EQ(loaded.entries(), store.entries());
    EXPECT_EQ(loaded.entries()[0].atMs, 1000);
    EXPECT_EQ(loaded.entries()[0].userLabel, "User Name");
    EXPECT_EQ(loaded.entries()[0].channelLabel, "<channel>");
}
TEST(SatorinoFlags, MergeKeepsNewestPerKeyAndIsDeterministic)
{
    FlagStore a, b;
    ASSERT_TRUE(a.toggle(record("100", "200", 1000)));
    auto newer = record("100", "200", 2000);
    newer.userLabel = "new label";
    ASSERT_TRUE(b.toggle(newer));
    ASSERT_TRUE(b.toggle(record("101", "200", 3000)));
    auto reverse = b;
    reverse.merge(a);
    a.merge(b);
    EXPECT_EQ(a.toJson(), reverse.toJson());
    EXPECT_EQ(a.entries().size(), 2);
    EXPECT_EQ(a.entries()[1].userLabel, "new label");
    a.merge(a);
    EXPECT_EQ(a.size(), 2);
}
TEST(SatorinoFlags, LoadingDuplicatesAndOversizedStateKeepsNewestWithinCaps)
{
    QJsonArray entries;
    for (int i = 1; i <= 130; ++i)
        entries.push_back(QJsonObject{
            {"roomId", "100"}, {"userId", QString::number(i)}, {"at", i}});
    entries.push_back(QJsonObject{{"roomId", "100"},
                                  {"userId", "130"},
                                  {"user", "newest"},
                                  {"at", 1000}});
    entries.push_back(
        QJsonObject{{"roomId", "bad"}, {"userId", "999"}, {"at", 2000}});
    const auto store = FlagStore::fromJson(QString::fromUtf8(
        QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}})
            .toJson()));
    EXPECT_EQ(store.size(), MAX_FLAGS_PER_ROOM);
    EXPECT_TRUE(store.contains("100", "130"));
    EXPECT_FALSE(store.contains("100", "1"));
    EXPECT_EQ(store.entries()[0].userLabel, "newest");
}
TEST(SatorinoFlags,
     EveryPriorityAndOptionCombinationPreservesMentionsAndRewards)
{
    const QColor chosen(237, 139, 99, 80);
    for (bool enabled : {false, true})
        for (bool flagged : {false, true})
            for (bool mark : {false, true})
                for (bool higher : {false, true})
                    for (bool ignore : {false, true})
                    {
                        const auto style = flagStyle(enabled, flagged, mark,
                                                     higher, ignore, chosen);
                        EXPECT_EQ(style.mark, enabled && flagged && mark);
                        EXPECT_EQ(style.background.has_value(),
                                  enabled && flagged && !higher && !ignore);
                        if (style.background)
                            EXPECT_EQ(*style.background, chosen);
                    }
    EXPECT_FALSE(
        flagStyle(true, true, true, false, false, QColor{}).background);
}
TEST(SatorinoFlags, DefaultsAndRegisteredSettingsPersistAlphaAndDormantFlags)
{
    EXPECT_TRUE(FLAGS_ENABLED_DEFAULT);
    EXPECT_TRUE(FLAGS_MARK_DEFAULT);
    EXPECT_EQ(QColor(FLAGS_COLOUR_DEFAULT), QColor(237, 139, 99, 80));
    auto manager = std::make_shared<pajlada::Settings::SettingManager>();
    manager->saveMethod =
        pajlada::Settings::SettingManager::SaveMethod::SaveManually;
    const auto option = pajlada::Settings::SettingOption::CompareBeforeSet;
    pajlada::Settings::Setting<QString> state(
        "/satorino/flags/state", FlagStore{}.toJson(), option, manager);
    pajlada::Settings::Setting<bool> enabled(
        "/satorino/flags/enabled", FLAGS_ENABLED_DEFAULT, option, manager);
    pajlada::Settings::Setting<QString> colour("/satorino/flags/colour",
                                               QString(FLAGS_COLOUR_DEFAULT),
                                               option, manager);
    FlagStore store;
    ASSERT_TRUE(store.toggle(record()));
    state.setValue(store.toJson());
    enabled.setValue(false);
    colour.setValue(QColor(10, 20, 30, 47).name(QColor::HexArgb));
    pajlada::Settings::Setting<QString> reloaded("/satorino/flags/state",
                                                 QString{}, option, manager);
    EXPECT_EQ(FlagStore::fromJson(reloaded.getValue()).entries(),
              store.entries());
    EXPECT_FALSE(enabled.getValue());
    EXPECT_EQ(QColor(colour.getValue()), QColor(10, 20, 30, 47));
    enabled.setValue(true);
    EXPECT_TRUE(
        FlagStore::fromJson(reloaded.getValue()).contains("100", "200"));
}

TEST(SatorinoFlags, MergeAndLoadEnforceTotalCapAcrossRooms)
{
    FlagStore a, b;
    for (int room = 1; room <= 4; ++room)
        for (int user = 1; user <= 100; ++user)
            ASSERT_TRUE(a.toggle(record(QString::number(room),
                                        QString::number(user), 1000 + user)));
    for (int room = 5; room <= 8; ++room)
        for (int user = 1; user <= 100; ++user)
            ASSERT_TRUE(b.toggle(record(QString::number(room),
                                        QString::number(user), 2000 + user)));
    a.merge(b);
    EXPECT_EQ(a.size(), MAX_FLAGS);
    const auto loaded = FlagStore::fromJson(a.toJson());
    EXPECT_EQ(loaded.size(), MAX_FLAGS);
    EXPECT_EQ(loaded.toJson(), a.toJson());
    EXPECT_TRUE(loaded.contains("8", "100"));
}

// satorino: popup decisions stay pure and use IDs from its own message context.
TEST(SatorinoFlags, PopupTargetUsesRoomAndAuthorIdsWithoutLabelFallbacks)
{
    const auto target = flagTarget("100", "93281234");
    ASSERT_TRUE(target);
    EXPECT_EQ(target->roomId, "100");
    EXPECT_EQ(target->userId, "93281234");
    FlagStore store;
    ASSERT_TRUE(store.toggle(record(target->roomId, target->userId)));
    EXPECT_FALSE(store.contains("101", target->userId));
    for (const auto &missing :
         {QString{}, QString("channel"), QString("0"), QString("kick:100")})
    {
        EXPECT_FALSE(flagTarget(missing, target->userId));
        EXPECT_FALSE(flagTarget(target->roomId, missing));
    }
}
TEST(SatorinoFlags,
     PopupStateTracksToggleAndMissingIdsAndMasterWithoutChangingStore)
{
    FlagStore store;
    auto state = flagControlState(store, "100", "93281234", true);
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(state.text, "Flag user");
    ASSERT_TRUE(store.toggle(record("100", "93281234")));
    state = flagControlState(store, "100", "93281234", true);
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(state.text, "Unflag user");
    const auto dormant = flagControlState(store, "100", "93281234", false);
    EXPECT_FALSE(dormant.enabled);
    EXPECT_EQ(dormant.text, "Unflag user");
    EXPECT_EQ(dormant.tooltip, "Local flags are off");
    for (const auto &ids : {std::make_pair(QString{}, QString("93281234")),
                            std::make_pair(QString("100"), QString{})})
    {
        state = flagControlState(store, ids.first, ids.second, true);
        EXPECT_FALSE(state.enabled);
        EXPECT_EQ(state.text, "Flag user");
        EXPECT_EQ(state.tooltip, "Waiting for channel/user ID");
    }
    EXPECT_EQ(store.size(), 1);
    ASSERT_TRUE(store.toggle(record("100", "93281234")));
    EXPECT_EQ(flagControlState(store, "100", "93281234", true).text,
              "Flag user");
}
TEST(SatorinoFlags, PopupLimitFeedbackMatchesStoreAndStillAllowsRemoval)
{
    FlagStore store;
    for (int user = 1; user <= MAX_FLAGS_PER_ROOM; ++user)
        ASSERT_TRUE(store.toggle(record("100", QString::number(user))));
    auto state = flagControlState(store, "100", "93281234", true);
    // A click displays the capacity reason.
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(state.text, "Flag user");
    EXPECT_TRUE(state.tooltip.contains("limit"));
    QString error;
    EXPECT_FALSE(store.toggle(record("100", "93281234"), &error));
    EXPECT_EQ(error, state.tooltip);
    state = flagControlState(store, "100", "1", true);
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(state.text, "Unflag user");
    EXPECT_FALSE(state.tooltip.contains("limit"));
    ASSERT_TRUE(store.toggle(record("100", "1")));
    EXPECT_FALSE(flagControlState(store, "100", "93281234", true)
                     .tooltip.contains("limit"));
    for (int room = 101; room <= 104; ++room)
        for (int user = 1; user <= MAX_FLAGS_PER_ROOM; ++user)
            ASSERT_TRUE(store.toggle(
                record(QString::number(room), QString::number(user))));
    ASSERT_TRUE(store.toggle(record("100", "1")));
    EXPECT_EQ(store.size(), MAX_FLAGS);
    state = flagControlState(store, "999", "93281234", true);
    EXPECT_TRUE(state.enabled);
    EXPECT_FALSE(store.toggle(record("999", "93281234"), &error));
    EXPECT_EQ(error, state.tooltip);
}
TEST(SatorinoFlags, NativeFlagHasNoViewerOrNonTwitchFallback)
{
    EXPECT_TRUE(flagControlVisible(true, true, FlagTargetRole::Regular));
    EXPECT_FALSE(flagControlVisible(true, false, FlagTargetRole::Regular));
    EXPECT_FALSE(flagControlVisible(false, true, FlagTargetRole::Regular));
}
