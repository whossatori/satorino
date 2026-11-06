// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/trains/SatorinoTrainProducer.hpp"
namespace chatterino::satorino {
namespace {
class NoDataTrainProducer final : public TrainProducer
{
public:
    bool available() const override
    {
        return false;
    }
    std::optional<TrainState> snapshot(const QString &, qint64) const override
    {
        return std::nullopt;
    }
};
}  // namespace
TrainProducer &trainProducer()
{
    static NoDataTrainProducer producer;
    return producer;
}
}  // namespace chatterino::satorino
