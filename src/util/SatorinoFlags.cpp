// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoFlags.hpp"
#include <QByteArray>
#include <QChar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace chatterino::satorino {
namespace {
QString label(QString text)
{
    text = text.left(80);
    for (auto &c : text)
        if (!c.isPrint())
            c = QChar(' ');
    return text.trimmed();
}
bool valid(const UserFlag &flag)
{
    return validFlagId(flag.roomId) && validFlagId(flag.userId) &&
           flag.atMs > 0 && flag.atMs <= 253402300799999LL;
}
bool newer(const UserFlag &a, const UserFlag &b)
{
    if (a.atMs != b.atMs)
        return a.atMs > b.atMs;
    if (a.channelLabel != b.channelLabel)
        return a.channelLabel > b.channelLabel;
    return a.userLabel > b.userLabel;
}
}  // namespace
bool validFlagId(const QString &id)
{
    return !id.isEmpty() && id.size() <= 32 && id.front() != u'0' &&
           std::all_of(id.begin(), id.end(), [](QChar c) {
               return c >= u'0' && c <= u'9';
           });
}
FlagStore FlagStore::fromJson(const QString &json)
{
    FlagStore store;
    if (json.size() > 2 * 1024 * 1024)
        return store;
    const auto bytes = json.toUtf8();
    if (bytes.size() > 2 * 1024 * 1024)
        return store;
    const auto doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject())
        return store;
    const auto obj = doc.object();
    if (obj.value("version").toDouble() != 1 || !obj.value("entries").isArray())
        return store;
    QVector<UserFlag> records;
    for (const auto &value : obj.value("entries").toArray())
    {
        if (records.size() >= 5000)
            break;
        const auto item = value.toObject();
        bool ok = false;
        auto at = item.value("at").toString().toLongLong(&ok);
        if (!ok && item.value("at").isDouble())
        {
            const auto n = item.value("at").toDouble();
            if (std::isfinite(n) && n > 0 && n <= 253402300799999.0 &&
                std::floor(n) == n)
                at = static_cast<qint64>(n);
        }
        records.push_back({item.value("roomId").toString(),
                           item.value("userId").toString(),
                           item.value("channel").toString(),
                           item.value("user").toString(), at});
    }
    store.assign(records);
    return store;
}
void FlagStore::assign(const QVector<UserFlag> &records)
{
    std::map<std::pair<QString, QString>, UserFlag> unique;
    for (auto record : records)
    {
        if (!valid(record))
            continue;
        record.channelLabel = label(record.channelLabel);
        record.userLabel = label(record.userLabel);
        const auto key = std::make_pair(record.roomId, record.userId);
        const auto it = unique.find(key);
        if (it == unique.end() || newer(record, it->second))
            unique[key] = record;
    }
    QVector<UserFlag> ordered;
    for (const auto &[key, record] : unique)
        ordered.push_back(record);
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
        if (a.atMs != b.atMs)
            return a.atMs > b.atMs;
        return std::make_pair(a.roomId, a.userId) <
               std::make_pair(b.roomId, b.userId);
    });
    this->records_.clear();
    std::map<QString, int> counts;
    for (const auto &record : ordered)
        if (this->records_.size() < static_cast<std::size_t>(MAX_FLAGS) &&
            counts[record.roomId] < MAX_FLAGS_PER_ROOM)
        {
            this->records_[{record.roomId, record.userId}] = record;
            ++counts[record.roomId];
        }
}
QString FlagStore::toJson() const
{
    QJsonArray entries;
    for (const auto &f : this->entries())
        entries.push_back(QJsonObject{{"roomId", f.roomId},
                                      {"userId", f.userId},
                                      {"channel", f.channelLabel},
                                      {"user", f.userLabel},
                                      {"at", QString::number(f.atMs)}});
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}})
            .toJson(QJsonDocument::Compact));
}
bool FlagStore::contains(const QString &room, const QString &user) const
{
    return this->records_.contains({room, user});
}
bool FlagStore::remove(const QString &room, const QString &user)
{
    return this->records_.erase({room, user}) != 0;
}
bool FlagStore::toggle(UserFlag flag, QString *error)
{
    if (error)
        error->clear();
    if (!valid(flag))
    {
        if (error)
            *error = "Channel/user ID or timestamp unavailable";
        return false;
    }
    if (this->remove(flag.roomId, flag.userId))
        return true;
    const auto reason = this->additionError(flag.roomId, flag.userId);
    if (!reason.isEmpty())
    {
        if (error)
            *error = reason;
        return false;
    }
    flag.channelLabel = label(flag.channelLabel);
    flag.userLabel = label(flag.userLabel);
    this->records_[{flag.roomId, flag.userId}] = flag;
    return true;
}
QString FlagStore::additionError(const QString &room, const QString &user) const
{
    if (!flagTarget(room, user))
        return "Waiting for channel/user ID";
    if (this->contains(room, user))
        return {};  // Removing an existing flag is always allowed at capacity.
    int roomCount = 0;
    for (const auto &[key, record] : this->records_)
        if (record.roomId == room)
            ++roomCount;
    if (this->size() >= MAX_FLAGS || roomCount >= MAX_FLAGS_PER_ROOM)
        return "Local flag limit reached (500 total, 100 per channel)";
    return {};
}
std::optional<FlagTarget> flagTarget(const QString &room, const QString &user)
{
    if (!validFlagId(room) || !validFlagId(user))
        return std::nullopt;
    return FlagTarget{room, user};
}
FlagControlState flagControlState(const FlagStore &store, const QString &room,
                                  const QString &user, bool masterEnabled)
{
    const bool ready = flagTarget(room, user).has_value();
    const bool flagged = ready && store.contains(room, user);
    const auto text = flagged ? "Unflag user" : "Flag user";
    if (!masterEnabled)
        return {false, text, "Local flags are off"};
    if (!ready)
        return {false, text, "Waiting for channel/user ID"};
    const auto error = store.additionError(room, user);
    // Keep a full-store control clickable so the reason can be shown at click time.
    return {true, text,
            error.isEmpty()
                ? QString("Local to this channel; nothing is sent to Twitch")
                : error};
}
void FlagStore::merge(const FlagStore &other)
{
    auto records = this->entries();
    records.append(other.entries());
    this->assign(records);
}
QVector<UserFlag> FlagStore::entries() const
{
    QVector<UserFlag> result;
    for (const auto &[key, record] : this->records_)
        result.push_back(record);
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        if (a.atMs != b.atMs)
            return a.atMs > b.atMs;
        return std::make_pair(a.roomId, a.userId) <
               std::make_pair(b.roomId, b.userId);
    });
    return result;
}
int FlagStore::size() const
{
    return static_cast<int>(this->records_.size());
}
FlagStyle flagStyle(bool enabled, bool flagged, bool showMark, bool higher,
                    bool ignore, const QColor &colour)
{
    if (!enabled || !flagged)
        return {};
    return {showMark, !higher && !ignore && colour.isValid()
                          ? std::optional<QColor>(colour)
                          : std::nullopt};
}
}  // namespace chatterino::satorino
