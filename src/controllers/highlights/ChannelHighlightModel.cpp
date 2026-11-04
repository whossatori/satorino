// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/highlights/ChannelHighlightModel.hpp"

#include "util/StandardItemHelper.hpp"

#include <QRegularExpression>

namespace chatterino::satorino {

ChannelHighlightModel::ChannelHighlightModel(
    SignalVector<ChannelHighlightPhrase> &phrases, QObject *parent)
    : SignalVectorModel<ChannelHighlightPhrase>(Count, parent)
    , phrases_(phrases)
{
    this->initialize(&phrases);
}

ChannelHighlightPhrase ChannelHighlightModel::getItemFromRow(
    std::vector<QStandardItem *> &row,
    const ChannelHighlightPhrase & /*original*/,
    std::optional<QModelIndex> /*index*/, std::optional<int> /*role*/)
{
    ChannelHighlightPhrase item;
    item.pattern = row[Pattern]->data(Qt::EditRole).toString();
    item.showInMentions = row[Mentions]->checkState() == Qt::Checked;
    item.hasAlert = row[Alert]->checkState() == Qt::Checked;
    item.hasSound = row[Sound]->checkState() == Qt::Checked;
    item.soundUrl = row[CustomSound]->data(Qt::UserRole).toUrl();
    item.color = row[Color]->data(Qt::DecorationRole).value<QColor>();
    item.isRegex = row[Regex]->checkState() == Qt::Checked;
    item.isCaseSensitive = row[CaseSensitive]->checkState() == Qt::Checked;
    this->getRowFromItem(item, row);
    return item;
}

void ChannelHighlightModel::getRowFromItem(const ChannelHighlightPhrase &item,
                                           std::vector<QStandardItem *> &row)
{
    setStringItem(row[Pattern], item.pattern);
    setBoolItem(row[Mentions], item.showInMentions);
    setBoolItem(row[Alert], item.hasAlert);
    setBoolItem(row[Sound], item.hasSound);
    setFilePathItem(row[CustomSound], item.soundUrl);
    row[CustomSound]->setToolTip(item.soundUrl.isEmpty()
                                     ? "Double-click to choose a sound file."
                                     : item.soundUrl.toDisplayString());
    setColorItem(row[Color], item.color);
    row[Color]->setToolTip("Double-click to choose a colour.");
    setBoolItem(row[Regex], item.isRegex);
    setBoolItem(row[CaseSensitive], item.isCaseSensitive);

    const QRegularExpression expression(item.pattern);
    const bool invalid = item.isRegex && !expression.isValid();
    row[Pattern]->setToolTip(invalid ? expression.errorString() : QString());
    row[Pattern]->setData(invalid ? QVariant(QColor(Qt::red)) : QVariant(),
                          Qt::ForegroundRole);
}

bool ChannelHighlightModel::setData(const QModelIndex &index,
                                    const QVariant &value, int role)
{
    const bool changed =
        SignalVectorModel<ChannelHighlightPhrase>::setData(index, value, role);
    if (changed)
    {
        // A regex toggle also updates the pattern cell's error/colour roles.
        Q_EMIT this->dataChanged(this->index(index.row(), 0),
                                 this->index(index.row(), Count - 1), {});
    }
    return changed;
}

bool ChannelHighlightModel::moveRows(const QModelIndex & /*sourceParent*/,
                                     int sourceRow, int count,
                                     const QModelIndex & /*destinationParent*/,
                                     int destinationChild)
{
    // EditableModelView's buttons and inherited row drag pass a target ROW,
    // not a Qt insertion offset. Publish remove/insert to every shared editor;
    // do not nest the base adapter's row notifications inside beginMoveRows.
    const int size = static_cast<int>(this->phrases_.raw().size());
    if (count != 1 || sourceRow < 0 || sourceRow >= size ||
        destinationChild < 0 || destinationChild >= size ||
        sourceRow == destinationChild)
    {
        return false;
    }
    const auto phrase = this->phrases_.raw()[sourceRow];
    this->phrases_.removeAt(sourceRow);
    this->phrases_.insert(phrase, destinationChild);
    return true;
}

}  // namespace chatterino::satorino
