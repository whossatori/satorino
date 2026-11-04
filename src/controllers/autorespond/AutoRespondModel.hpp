// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/SignalVectorModel.hpp"
#include "controllers/autorespond/AutoRespondData.hpp"

namespace chatterino::satorino {

class AutoRespondTriggerModel : public SignalVectorModel<AutoRespondTrigger>
{
public:
    enum Column {
        Enabled,
        Label,
        Channel,
        Pattern,
        Regex,
        CaseSensitive,
        Response,
        Sound,
        CustomSound,
        Cooldown,
        Delay,
        Count
    };
    explicit AutoRespondTriggerModel(SignalVector<AutoRespondTrigger> &rows);
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;
    bool moveRows(const QModelIndex &, int source, int count,
                  const QModelIndex &, int destination) override;

protected:
    AutoRespondTrigger getItemFromRow(std::vector<QStandardItem *> &row,
                                      const AutoRespondTrigger &original,
                                      std::optional<QModelIndex>,
                                      std::optional<int>) override;
    void getRowFromItem(const AutoRespondTrigger &item,
                        std::vector<QStandardItem *> &row) override;

private:
    SignalVector<AutoRespondTrigger> &rows_;
};

class AutoRespondTimerModel : public SignalVectorModel<AutoRespondTimer>
{
public:
    enum Column { Enabled, Label, Channel, Message, Time, Days, Count };
    explicit AutoRespondTimerModel(SignalVector<AutoRespondTimer> &rows);
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;
    bool moveRows(const QModelIndex &, int source, int count,
                  const QModelIndex &, int destination) override;

protected:
    AutoRespondTimer getItemFromRow(std::vector<QStandardItem *> &row,
                                    const AutoRespondTimer &original,
                                    std::optional<QModelIndex>,
                                    std::optional<int>) override;
    void getRowFromItem(const AutoRespondTimer &item,
                        std::vector<QStandardItem *> &row) override;

private:
    SignalVector<AutoRespondTimer> &rows_;
};

}  // namespace chatterino::satorino
