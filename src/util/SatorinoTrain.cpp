// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "util/SatorinoTrain.hpp"

#include <QChar>
#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace chatterino::satorino {
namespace {
std::size_t index(TrainType type)
{
    const auto i = static_cast<std::size_t>(type);
    return i < TRAIN_TYPE_COUNT ? i
                                : static_cast<std::size_t>(TrainType::Other);
}
constexpr std::array<const char *, TRAIN_TYPE_COUNT> KEYS{
    "regular", "golden_kappa", "treasure", "shared",
    "mythic",  "community",    "other"};
constexpr std::array<const char *, TRAIN_TYPE_COUNT> NAMES{
    "Normal", "Golden Kappa", "Treasure",       "Shared",
    "Mythic", "Community",    "Other / unknown"};
// Golden wins across aggregate children; the remaining ordering is stable presentation only.
constexpr std::array<int, TRAIN_TYPE_COUNT> PRIORITY{1, 7, 5, 3, 6, 4, 2};
bool validRoom(const QString &id)
{
    return !id.isEmpty() && id.size() <= 128 &&
           std::all_of(id.begin(), id.end(), [](QChar c) {
               return c >= u'0' && c <= u'9';
           });
}
}  // namespace
QString trainTypeKey(TrainType type)
{
    return QString::fromLatin1(KEYS[index(type)]);
}
QString trainTypeName(TrainType type)
{
    return QString::fromLatin1(NAMES[index(type)]);
}
TrainType trainType(const QString &name, bool shared)
{
    // Do not infer a type when the producer did not supply one, or from emote names.
    for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
        if (name == QLatin1String(KEYS[i]))
        {
            const auto type = static_cast<TrainType>(i);
            return type == TrainType::Regular && shared ? TrainType::Shared
                                                        : type;
        }
    return TrainType::Other;
}
TrainColours defaultTrainColours()
{
    return {{QColor("#23c9a9"), QColor("#ffd23f"), QColor("#ff934f"),
             QColor("#7297ff"), QColor("#d781ef"), QColor("#ef78a8"),
             QColor("#a6b2c5")}};
}
QJsonObject trainColoursToJson(const TrainColours &colours)
{
    QJsonObject result;
    for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
        result.insert(QString::fromLatin1(KEYS[i]),
                      colours.values[i].name(QColor::HexArgb));
    return result;
}
TrainColours trainColoursFromJson(const QJsonObject &json)
{
    auto result = defaultTrainColours();
    for (std::size_t i = 0; i < TRAIN_TYPE_COUNT; ++i)
    {
        const QColor colour(
            json.value(QString::fromLatin1(KEYS[i])).toString());
        if (colour.isValid())
            result.values[i] = colour;
    }
    return result;
}
QString trainDataStatus(bool enabled, bool available)
{
    return !enabled     ? QString("Train indicators: off")
           : !available ? QString("Train data is not available.")
                        : QString("Train indicators: waiting for channel data");
}
bool TrainState::running(qint64 nowMs) const
{
    return phase == TrainPhase::Running && nowMs > 0 && nowMs >= updatedAtMs &&
           expiresAtMs > nowMs;
}
void TrainTracker::setEnabled(bool enabled)
{
    this->enabled_ = enabled;
    if (!enabled)
        this->states_.clear();
}
bool TrainTracker::observe(const QString &roomId, const TrainUpdate &update,
                           qint64 nowMs)
{
    if (!this->enabled_ || !validRoom(roomId) || update.id.isEmpty() ||
        update.id.size() > 128 || nowMs <= 0 ||
        nowMs > std::numeric_limits<qint64>::max() - TRAIN_FALLBACK_MS ||
        update.atMs <= 0 || update.atMs > nowMs ||
        update.atMs < nowMs - TRAIN_FALLBACK_MS ||
        (update.kind != TrainEventKind::End && update.level < 1))
        return false;
    auto it = this->states_.find(roomId);
    if (it == this->states_.end() && this->states_.size() >= MAX_TRAIN_CHANNELS)
        return false;
    const bool same = it != this->states_.end() && it->second.id == update.id;
    if (it != this->states_.end())
    {
        const auto &old = it->second;
        if (update.atMs < old.updatedAtMs ||
            (same && (old.phase == TrainPhase::Ended ||
                      old.phase == TrainPhase::Expired)) ||
            (!same && update.kind != TrainEventKind::Start))
            return false;
        if (same && update.kind != TrainEventKind::End &&
            update.level < old.level)
            return false;
    }
    TrainState next = same ? it->second : TrainState{};
    next.id = update.id;
    next.updatedAtMs = update.atMs;
    next.type = static_cast<TrainType>(index(update.type));
    next.level = update.level;
    if (update.kind == TrainEventKind::End)
        next.phase = TrainPhase::Ended;
    else
    {
        const auto cap = update.atMs + TRAIN_FALLBACK_MS;
        if (update.expiresAtMs)
            next.expiresAtMs = std::clamp(*update.expiresAtMs, qint64(0), cap);
        else if (!same || update.kind == TrainEventKind::LevelUp)
            next.expiresAtMs = cap;
        // Progress without a new expiry never slides the deadline.
        next.phase = next.expiresAtMs > nowMs ? TrainPhase::Running
                                              : TrainPhase::Expired;
    }
    this->states_[roomId] = next;
    return true;
}
QStringList TrainTracker::expire(qint64 nowMs)
{
    QStringList changed;
    for (auto &[room, state] : this->states_)
        if (state.phase == TrainPhase::Running && state.expiresAtMs <= nowMs)
        {
            state.phase = TrainPhase::Expired;
            changed.push_back(room);
        }
    return changed;
}
void TrainTracker::retain(const std::set<QString> &rooms)
{
    std::erase_if(this->states_, [&rooms](const auto &entry) {
        return !rooms.contains(entry.first);
    });
}
TrainState TrainTracker::state(const QString &roomId) const
{
    const auto it = this->states_.find(roomId);
    return it == this->states_.end() ? TrainState{} : it->second;
}
std::optional<qint64> TrainTracker::nextExpiry() const
{
    std::optional<qint64> result;
    for (const auto &[room, state] : this->states_)
        if (state.phase == TrainPhase::Running &&
            (!result || state.expiresAtMs < *result))
            result = state.expiresAtMs;
    return result;
}
std::size_t TrainTracker::size() const
{
    return this->states_.size();
}
std::optional<HighlightedIndicator> highlightedLiveIndicator(
    bool live, const std::optional<QColor> &game, bool gameStar,
    bool trainsEnabled, const QVector<TrainState> &trains,
    const TrainColours &colours, qint64 nowMs)
{
    const TrainState *chosen = nullptr;
    if (trainsEnabled && live)
        for (const auto &train : trains)
            if (train.running(nowMs) &&
                (!chosen ||
                 PRIORITY[index(train.type)] > PRIORITY[index(chosen->type)]))
                chosen = &train;
    const auto kind = liveIndicator(live, game.has_value(), chosen != nullptr);
    if (kind == LiveIndicator::Train)
        return HighlightedIndicator{
            kind,
            indicatorShape(kind, false, chosen->type == TrainType::GoldenKappa),
            colours.values[index(chosen->type)]};
    if (kind == LiveIndicator::Game)
        return HighlightedIndicator{kind, indicatorShape(kind, gameStar),
                                    *game};
    // Stock dot / rerun / no indicator keeps its original paint path.
    return std::nullopt;
}
}  // namespace chatterino::satorino
