// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoIndicator.hpp"

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>

namespace chatterino::satorino {
// Shared is a presentation category for a regular shared train; named special types take precedence.
enum class TrainType {
    Regular,
    GoldenKappa,
    Treasure,
    Shared,
    Mythic,
    Community,
    Other,
    Count
};
inline constexpr std::size_t TRAIN_TYPE_COUNT =
    static_cast<std::size_t>(TrainType::Count);
inline constexpr bool TRAIN_ENABLED_DEFAULT = true;
inline constexpr qint64 TRAIN_FALLBACK_MS = 10 * 60 * 1000;
inline constexpr std::size_t MAX_TRAIN_CHANNELS = 512;
inline constexpr auto TRAIN_ENABLED_PATH = "/satorino/live/train/enabled";
inline constexpr auto TRAIN_COLOURS_PATH = "/satorino/live/train/colours";
QString trainTypeKey(TrainType type);
QString trainTypeName(TrainType type);
// These are explicit model labels, not a speculative network parser. Unknown stays Other.
TrainType trainType(const QString &name, bool shared = false);
struct TrainColours {
    std::array<QColor, TRAIN_TYPE_COUNT> values;
    bool operator==(const TrainColours &) const = default;
};
TrainColours defaultTrainColours();
QJsonObject trainColoursToJson(const TrainColours &colours);
TrainColours trainColoursFromJson(const QJsonObject &json);
QString trainDataStatus(bool enabled, bool available);

enum class TrainPhase { NoData, Running, Ended, Expired };
enum class TrainEventKind { Start, Progress, LevelUp, End };
struct TrainUpdate {
    QString id;
    TrainEventKind kind = TrainEventKind::Start;
    TrainType type = TrainType::Other;
    int level = 1;
    qint64 atMs = 0;
    std::optional<qint64> expiresAtMs;
};
struct TrainState {
    QString id;
    TrainPhase phase = TrainPhase::NoData;
    TrainType type = TrainType::Other;
    int level = 0;
    qint64 updatedAtMs = 0;
    qint64 expiresAtMs = 0;
    bool running(qint64 nowMs) const;
};
// Injected times, no IO/timers. At most one state/tombstone for each retained open room.
class TrainTracker
{
public:
    void setEnabled(bool enabled);
    bool observe(const QString &roomId, const TrainUpdate &update,
                 qint64 nowMs);
    QStringList expire(qint64 nowMs);
    void retain(const std::set<QString> &rooms);
    TrainState state(const QString &roomId) const;
    std::optional<qint64> nextExpiry() const;
    std::size_t size() const;

private:
    bool enabled_ = TRAIN_ENABLED_DEFAULT;
    std::map<QString, TrainState> states_;
};
std::optional<HighlightedIndicator> trainIndicatorStyle(
    bool live, bool trainsEnabled, const QVector<TrainState> &trains,
    const TrainColours &colours, qint64 nowMs);
}  // namespace chatterino::satorino
