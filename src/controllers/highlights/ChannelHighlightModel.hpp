// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/SignalVectorModel.hpp"
#include "controllers/highlights/ChannelHighlightData.hpp"

namespace chatterino::satorino {

// The phrase-table adapter for the upstream HighlightPhrase schema. The active
// global Model now presents three summary columns, not this eight-column table.
class ChannelHighlightModel : public SignalVectorModel<ChannelHighlightPhrase>
{
public:
    enum Column {
        Pattern,
        Mentions,
        Alert,
        Sound,
        CustomSound,
        Color,
        Regex,
        CaseSensitive,
        Count,
    };

    explicit ChannelHighlightModel(
        SignalVector<ChannelHighlightPhrase> &phrases,
        QObject *parent = nullptr);
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent,
                  int destinationChild) override;

protected:
    ChannelHighlightPhrase getItemFromRow(
        std::vector<QStandardItem *> &row,
        const ChannelHighlightPhrase &original,
        std::optional<QModelIndex> index, std::optional<int> role) override;
    void getRowFromItem(const ChannelHighlightPhrase &item,
                        std::vector<QStandardItem *> &row) override;

private:
    SignalVector<ChannelHighlightPhrase> &phrases_;
};

}  // namespace chatterino::satorino
