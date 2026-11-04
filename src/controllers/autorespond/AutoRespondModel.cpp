// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondModel.hpp"

#include "util/StandardItemHelper.hpp"

#include <algorithm>

namespace chatterino::satorino {
namespace {

template <typename Rule>
bool moveRule(SignalVector<Rule> &rows, int source, int count, int destination)
{
    const auto size = static_cast<int>(rows.raw().size());
    if (count != 1 || source < 0 || source >= size || destination < 0 ||
        destination >= size || source == destination)
        return false;
    const auto item = rows.raw()[source];
    rows.removeAt(source);
    rows.insert(item, destination);
    return true;
}

void channelItem(QStandardItem *item, const QString &channel, bool timer)
{
    setStringItem(item, channel == "*" ? (timer ? "All connected channels"
                                                : "All channels")
                                       : channel);
    item->setData(channel, Qt::UserRole);
    item->setToolTip(
        "Choose a Twitch or Kick channel. Saved closed channels are retained.");
}

}  // namespace

AutoRespondTriggerModel::AutoRespondTriggerModel(
    SignalVector<AutoRespondTrigger> &rows)
    : SignalVectorModel<AutoRespondTrigger>(Count)
    , rows_(rows)
{
    this->initialize(&rows);
}

AutoRespondTrigger AutoRespondTriggerModel::getItemFromRow(
    std::vector<QStandardItem *> &row, const AutoRespondTrigger &original,
    std::optional<QModelIndex>, std::optional<int>)
{
    auto item = original;
    item.enabled = row[Enabled]->checkState() == Qt::Checked;
    item.label = row[Label]->text();
    item.channel = row[Channel]->data(Qt::UserRole).toString();
    item.pattern = row[Pattern]->text();
    item.regex = row[Regex]->checkState() == Qt::Checked;
    item.caseSensitive = row[CaseSensitive]->checkState() == Qt::Checked;
    item.response = row[Response]->text();
    item.sound = row[Sound]->checkState() == Qt::Checked;
    item.customSound = row[CustomSound]->data(Qt::UserRole).toUrl().toString();
    item.cooldownSeconds =
        std::clamp(row[Cooldown]->data(Qt::EditRole).toInt(), 1, 86400);
    item.delayMs =
        std::clamp(row[Delay]->data(Qt::EditRole).toInt(), 0, 3600000);
    this->getRowFromItem(item, row);
    return item;
}

void AutoRespondTriggerModel::getRowFromItem(const AutoRespondTrigger &item,
                                             std::vector<QStandardItem *> &row)
{
    setBoolItem(row[Enabled], item.enabled);
    setStringItem(row[Label], item.label);
    channelItem(row[Channel], item.channel, false);
    setStringItem(row[Pattern], item.pattern);
    setBoolItem(row[Regex], item.regex);
    setBoolItem(row[CaseSensitive], item.caseSensitive);
    setStringItem(row[Response], item.response);
    setBoolItem(row[Sound], item.sound);
    setFilePathItem(row[CustomSound], QUrl(item.customSound));
    row[CustomSound]->setToolTip(
        item.customSound.isEmpty()
            ? "Double-click to choose a sound; empty uses the default sound."
            : item.customSound);
    setStringItem(row[Cooldown], QString::number(item.cooldownSeconds));
    setStringItem(row[Delay], QString::number(item.delayMs));
    const auto expression = autoRespondExpression(item);
    const auto error =
        expression.isValid() ? QString() : expression.errorString();
    row[Pattern]->setToolTip(error);
    row[Pattern]->setData(
        error.isEmpty() ? QVariant() : QVariant(QColor(Qt::red)),
        Qt::ForegroundRole);
    row[Response]->setToolTip("Placeholders: {user}, {channel}, {match}, "
                              "{1}..{9}. One line, up to 500 bytes.");
}

bool AutoRespondTriggerModel::setData(const QModelIndex &index,
                                      const QVariant &value, int role)
{
    if (!index.isValid())
        return false;
    const bool changed =
        SignalVectorModel<AutoRespondTrigger>::setData(index, value, role);
    if (changed)
        Q_EMIT this->dataChanged(this->index(index.row(), 0),
                                 this->index(index.row(), Count - 1), {});
    return changed;
}
bool AutoRespondTriggerModel::moveRows(const QModelIndex &, int source,
                                       int count, const QModelIndex &,
                                       int destination)
{
    return moveRule(this->rows_, source, count, destination);
}

AutoRespondTimerModel::AutoRespondTimerModel(
    SignalVector<AutoRespondTimer> &rows)
    : SignalVectorModel<AutoRespondTimer>(Count)
    , rows_(rows)
{
    this->initialize(&rows);
}
AutoRespondTimer AutoRespondTimerModel::getItemFromRow(
    std::vector<QStandardItem *> &row, const AutoRespondTimer &original,
    std::optional<QModelIndex>, std::optional<int>)
{
    auto item = original;
    item.enabled = row[Enabled]->checkState() == Qt::Checked;
    item.label = row[Label]->text();
    item.channel = row[Channel]->data(Qt::UserRole).toString();
    item.message = row[Message]->text();
    item.time = row[Time]->text();
    item.days = row[Days]->data(Qt::UserRole).toInt() & 127;
    this->getRowFromItem(item, row);
    return item;
}
void AutoRespondTimerModel::getRowFromItem(const AutoRespondTimer &item,
                                           std::vector<QStandardItem *> &row)
{
    setBoolItem(row[Enabled], item.enabled);
    setStringItem(row[Label], item.label);
    channelItem(row[Channel], item.channel, true);
    setStringItem(row[Message], item.message);
    setStringItem(row[Time], item.time);
    const bool valid = QTime::fromString(item.time, "HH:mm").isValid();
    row[Time]->setToolTip(valid ? "Local time, 24-hour clock."
                                : "Enter a valid HH:mm time.");
    row[Time]->setData(valid ? QVariant() : QVariant(QColor(Qt::red)),
                       Qt::ForegroundRole);
    const QStringList names{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    QStringList selected;
    for (int i = 0; i < 7; ++i)
        if (item.days & (1 << i))
            selected << names[i];
    setStringItem(row[Days],
                  selected.isEmpty() ? "Never" : selected.join(", "));
    row[Days]->setData(item.days, Qt::UserRole);
    row[Days]->setToolTip(
        "Double-click to select days. Missed times are skipped.");
}
bool AutoRespondTimerModel::setData(const QModelIndex &index,
                                    const QVariant &value, int role)
{
    if (!index.isValid())
        return false;
    const bool changed =
        SignalVectorModel<AutoRespondTimer>::setData(index, value, role);
    if (changed)
        Q_EMIT this->dataChanged(this->index(index.row(), 0),
                                 this->index(index.row(), Count - 1), {});
    return changed;
}
bool AutoRespondTimerModel::moveRows(const QModelIndex &, int source, int count,
                                     const QModelIndex &, int destination)
{
    return moveRule(this->rows_, source, count, destination);
}
}  // namespace chatterino::satorino
