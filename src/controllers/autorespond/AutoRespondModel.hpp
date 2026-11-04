// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once
#include "controllers/autorespond/AutoRespondSettings.hpp"
#include <QAbstractTableModel>
#include <QPointer>
#include <optional>

namespace chatterino::satorino {
class AutoRespondModel : public QAbstractTableModel
{
public:
    enum Column { Enabled, Label, Type, Channel, When, Response, Count };
    explicit AutoRespondModel(AutoRespondSettings &settings,
                              QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    bool setHeaderData(int, Qt::Orientation, const QVariant &, int) override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;
    bool removeRows(int row, int count,
                    const QModelIndex &parent = {}) override;
    bool moveRows(const QModelIndex &, int source, int count,
                  const QModelIndex &, int destination) override;
    std::optional<AutoRespondRule> ruleAt(int row) const;
    int rowForId(const QString &id) const;
    bool updateRule(const AutoRespondRule &rule);
    void addRule(AutoRespondType type = AutoRespondType::Trigger);

private:
    void scheduleReload();
    QPointer<AutoRespondSettings> settings_;
    std::vector<AutoRespondRule> rows_;
    bool reloadPending_ = false;
    pajlada::Signals::SignalHolder connections_;
};
}  // namespace chatterino::satorino
