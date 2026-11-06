// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include <QColor>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <map>
#include <optional>
#include <utility>

namespace chatterino::satorino {
inline constexpr bool FLAGS_ENABLED_DEFAULT = true;
inline constexpr bool FLAGS_MARK_DEFAULT = true;
inline constexpr auto FLAGS_COLOUR_DEFAULT = "#50ed8b63";
inline constexpr int MAX_FLAGS = 500;
inline constexpr int MAX_FLAGS_PER_ROOM = 100;
struct UserFlag {
    QString roomId;
    QString userId;
    QString channelLabel;
    QString userLabel;
    qint64 atMs = 0;
    bool operator==(const UserFlag &) const = default;
};
bool validFlagId(const QString &id);
class FlagStore
{
public:
    static FlagStore fromJson(const QString &json);
    QString toJson() const;
    bool contains(const QString &roomId, const QString &userId) const;
    // Toggle removes an existing key even at capacity. Invalid/full adds leave the store unchanged.
    bool toggle(UserFlag flag, QString *error = nullptr);
    QString additionError(const QString &roomId, const QString &userId) const;
    bool remove(const QString &roomId, const QString &userId);
    void merge(const FlagStore &other);
    QVector<UserFlag> entries() const;
    int size() const;

private:
    void assign(const QVector<UserFlag> &records);
    std::map<std::pair<QString, QString>, UserFlag> records_;
};
// satorino: pure local highlight policy, isolated from native suspicious status.
struct FlagTarget {
    QString roomId;
    QString userId;
};
std::optional<FlagTarget> flagTarget(const QString &roomId,
                                     const QString &userId);
struct FlagControlState {
    bool enabled = false;
    QString text;
    QString tooltip;
};
FlagControlState flagControlState(const FlagStore &store, const QString &roomId,
                                  const QString &userId, bool masterEnabled);
struct FlagStyle {
    bool mark = false;
    std::optional<QColor> background;
};
FlagStyle flagStyle(bool enabled, bool flagged, bool showMark,
                    bool higherPriority, bool ignoreHighlights,
                    const QColor &colour);
}  // namespace chatterino::satorino
