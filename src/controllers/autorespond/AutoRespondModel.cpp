// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondModel.hpp"
#include <QColor>
#include <QStringList>
#include <algorithm>
#include <QTimer>

namespace chatterino::satorino {
AutoRespondModel::AutoRespondModel(AutoRespondSettings &settings,
                                   QObject *parent)
    : QAbstractTableModel(parent)
    , settings_(settings)
    , rows_(settings.rows.raw())
{
    this->connections_.managedConnect(settings.changed, [this] {
        this->scheduleReload();
    });
}
void AutoRespondModel::scheduleReload()
{
    if (this->reloadPending_)
        return;
    this->reloadPending_ = true;
    QTimer::singleShot(0, this, [this] {
        this->reloadPending_ = false;
        const auto replacement = this->settings_.rows.raw();
        if (replacement == this->rows_)
            return;
        this->beginResetModel();
        this->rows_ = replacement;
        this->endResetModel();
    });
}
int AutoRespondModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(this->rows_.size());
}
int AutoRespondModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : Count;
}
std::optional<AutoRespondRule> AutoRespondModel::ruleAt(int row) const
{
    if (row < 0 || row >= this->rowCount())
        return {};
    return this->rows_[row];
}
int AutoRespondModel::rowForId(const QString &id) const
{
    for (int i = 0; i < this->rowCount(); ++i)
        if (this->rows_[i].id == id)
            return i;
    return -1;
}
QVariant AutoRespondModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() < 0 || index.column() >= Count)
        return {};
    const auto item = this->ruleAt(index.row());
    if (!item)
        return {};
    const auto &r = *item;
    if (role == Qt::CheckStateRole && index.column() == Enabled)
        return r.enabled ? Qt::Checked : Qt::Unchecked;
    const bool timer = r.type == AutoRespondType::Timer;
    const auto expression = autoRespondExpression(r);
    const bool invalid = !timer && r.regex && !expression.isValid();
    if (role == Qt::ForegroundRole && index.column() == When && invalid)
        return QColor(Qt::red);
    if (role == Qt::ToolTipRole)
    {
        if (index.column() == When && invalid)
            return expression.errorString();
        if (index.column() == When)
            return timer ? r.time : r.pattern;
        if (index.column() == Channel)
            return r.channel;
        if (index.column() == Response)
            return r.response;
        if (index.column() == Label)
            return r.label;
    }
    if (role == Qt::UserRole && index.column() == Channel)
        return r.channel;
    if (role == Qt::EditRole && index.column() == Type)
        return static_cast<int>(r.type);
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};
    switch (index.column())
    {
        case Label:
            return r.label;
        case Type:
            return timer ? "Timer" : "Trigger";
        case Channel:
            return r.channel == "*"
                       ? (timer ? "All connected channels" : "All channels")
                       : r.channel;
        case When:
            return timer ? r.time : r.pattern;
        case Response:
            return r.response;
        default:
            return {};
    }
}
Qt::ItemFlags AutoRespondModel::flags(const QModelIndex &index) const
{
    if (!this->ruleAt(index.row()) || index.column() < 0 ||
        index.column() >= Count)
        return Qt::NoItemFlags;
    auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return flags | (index.column() == Enabled ? Qt::ItemIsUserCheckable
                                              : Qt::ItemIsEditable);
}
QVariant AutoRespondModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    const QStringList titles{"Enabled", "Label", "Type",
                             "Channel", "When",  "Response/Message"};
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole ||
        section < 0 || section >= Count)
        return {};
    return titles[section];
}
bool AutoRespondModel::setHeaderData(int, Qt::Orientation, const QVariant &,
                                     int)
{
    return false;
}
bool AutoRespondModel::setData(const QModelIndex &index, const QVariant &value,
                               int role)
{
    auto item = this->ruleAt(index.row());
    if (!item || !index.isValid())
        return false;
    auto &r = *item;
    if (index.column() == Enabled && role == Qt::CheckStateRole)
        r.enabled = value.toInt() == Qt::Checked;
    else if (role == Qt::EditRole || role == Qt::UserRole)
    {
        switch (index.column())
        {
            case Label:
                r.label = value.toString();
                break;
            case Type:
                r.type = value.toInt() == 1 ? AutoRespondType::Timer
                                            : AutoRespondType::Trigger;
                break;
            case Channel:
                r.channel = value.toString();
                break;
            case When:
                if (r.type == AutoRespondType::Timer)
                    r.time = value.toString();
                else
                    r.pattern = value.toString();
                break;
            case Response:
                r.response = value.toString();
                break;
            default:
                return false;
        }
    }
    else
        return false;
    return this->updateRule(r);
}
bool AutoRespondModel::updateRule(const AutoRespondRule &rule)
{
    const int row = this->rowForId(rule.id);
    if (row < 0)
        return false;
    auto latest = this->settings_.rules.getValueCopy();
    const auto found = std::ranges::find(latest, rule.id, &AutoRespondRule::id);
    if (found == latest.end() || *found != this->rows_[row])
    {
        this->scheduleReload();
        return false;
    }
    if (*found == rule)
        return true;
    *found = rule;
    this->rows_[row] = rule;
    this->settings_.rules.setValue(latest);
    Q_EMIT this->dataChanged(this->index(row, 0), this->index(row, Count - 1),
                             {});
    return true;
}
void AutoRespondModel::addRule(AutoRespondType type)
{
    if (this->settings_.rows.raw() != this->rows_)
    {
        this->scheduleReload();
        return;
    }
    const int row = this->rowCount();
    this->beginInsertRows({}, row, row);
    this->rows_.push_back(newAutoRespondRule(type));
    this->endInsertRows();
    this->settings_.rules.setValue(this->rows_);
}
bool AutoRespondModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count != 1 || !this->ruleAt(row) ||
        this->settings_.rows.raw() != this->rows_)
        return false;
    this->beginRemoveRows({}, row, row);
    this->rows_.erase(this->rows_.begin() + row);
    this->endRemoveRows();
    this->settings_.rules.setValue(this->rows_);
    return true;
}
bool AutoRespondModel::moveRows(const QModelIndex &, int source, int count,
                                const QModelIndex &, int destination)
{
    if (count != 1 || !this->ruleAt(source) || !this->ruleAt(destination) ||
        source == destination || this->settings_.rows.raw() != this->rows_)
        return false;
    // EditableModelView buttons pass a target row rather than Qt's insertion offset.
    const int insertion = destination > source ? destination + 1 : destination;
    if (!this->beginMoveRows({}, source, source, {}, insertion))
        return false;
    auto item = this->rows_[source];
    this->rows_.erase(this->rows_.begin() + source);
    this->rows_.insert(this->rows_.begin() + destination, std::move(item));
    this->endMoveRows();
    this->settings_.rules.setValue(this->rows_);
    return true;
}
}  // namespace chatterino::satorino
