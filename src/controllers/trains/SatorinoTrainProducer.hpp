// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include "util/SatorinoTrain.hpp"

#include <pajlada/signals/signal.hpp>
#include <optional>

namespace chatterino::satorino {
// GUI-only producer contract. A real producer must retain only open rooms, clear on
// disable/close, expire missed ends with a one-shot timer, and emit changed afterwards.
// Current implementation has no transport, retained state, timers or subscriptions.
class TrainProducer
{
public:
    virtual ~TrainProducer() = default;
    virtual bool available() const = 0;
    virtual std::optional<TrainState> snapshot(const QString &roomId,
                                               qint64 nowMs) const = 0;
    pajlada::Signals::Signal<const QString &> changed;
};
TrainProducer &trainProducer();
}  // namespace chatterino::satorino
