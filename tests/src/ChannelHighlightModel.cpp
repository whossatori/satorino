// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/highlights/ChannelHighlightModel.hpp"

#include "Test.hpp"

using namespace chatterino;
using namespace chatterino::satorino;

TEST(ChannelHighlightModel,
     SharedEditorsRetainAllOptionsAcrossEditsMovesAndDeletes)
{
    SignalVector<ChannelHighlightPhrase> phrases;
    phrases.append(ChannelHighlightPhrase::createDefault());
    auto other = ChannelHighlightPhrase::createDefault();
    other.pattern = "other";
    phrases.append(other);
    ChannelHighlightModel first(phrases);
    ChannelHighlightModel second(phrases);

    ASSERT_TRUE(first.setData(first.index(0, ChannelHighlightModel::Pattern),
                              "updated", Qt::EditRole));
    EXPECT_EQ(second
                  .data(second.index(0, ChannelHighlightModel::Pattern),
                        Qt::DisplayRole)
                  .toString(),
              "updated");
    const auto file = QUrl::fromLocalFile("C:/sounds/custom.wav");
    ASSERT_TRUE(
        first.setData(first.index(0, ChannelHighlightModel::CustomSound), file,
                      Qt::UserRole));
    ASSERT_TRUE(first.setData(first.index(0, ChannelHighlightModel::Sound),
                              Qt::Checked, Qt::CheckStateRole));
    ASSERT_TRUE(first.setData(first.index(0, ChannelHighlightModel::Sound),
                              Qt::Unchecked, Qt::CheckStateRole));
    EXPECT_FALSE(phrases.raw()[0].hasSound);
    EXPECT_EQ(phrases.raw()[0].soundUrl, file);
    const QColor color(10, 20, 30, 40);
    ASSERT_TRUE(first.setData(first.index(0, ChannelHighlightModel::Color),
                              color, Qt::DecorationRole));
    EXPECT_EQ(second
                  .data(second.index(0, ChannelHighlightModel::Color),
                        Qt::DecorationRole)
                  .value<QColor>(),
              color);

    ASSERT_TRUE(first.moveRows({}, 0, 1, {}, 1));
    EXPECT_EQ(phrases.raw()[1].pattern, "updated");
    EXPECT_EQ(phrases.raw()[1].soundUrl, file);
    EXPECT_EQ(second
                  .data(second.index(1, ChannelHighlightModel::Pattern),
                        Qt::DisplayRole)
                  .toString(),
              "updated");
    ASSERT_TRUE(second.removeRow(1));
    EXPECT_EQ(first.rowCount({}), 1);
    EXPECT_EQ(phrases.raw()[0].pattern, "other");
}

TEST(ChannelHighlightModel, InvalidRegexErrorAppearsWhenRegexIsEnabled)
{
    SignalVector<ChannelHighlightPhrase> phrases;
    auto phrase = ChannelHighlightPhrase::createDefault();
    phrase.pattern = "[";
    phrases.append(phrase);
    ChannelHighlightModel model(phrases);
    const auto pattern = model.index(0, ChannelHighlightModel::Pattern);
    EXPECT_TRUE(model.data(pattern, Qt::ToolTipRole).toString().isEmpty());
    ASSERT_TRUE(model.setData(model.index(0, ChannelHighlightModel::Regex),
                              Qt::Checked, Qt::CheckStateRole));
    EXPECT_FALSE(model.data(pattern, Qt::ToolTipRole).toString().isEmpty());
}
