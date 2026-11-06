// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include "util/SatorinoTrain.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <rapidjson/document.h>

namespace pajlada {
template <>
struct Serialize<chatterino::satorino::TrainColours> {
    static rapidjson::Value get(
        const chatterino::satorino::TrainColours &colours,
        rapidjson::Document::AllocatorType &allocator)
    {
        rapidjson::Value result(rapidjson::kObjectType);
        const auto json = chatterino::satorino::trainColoursToJson(colours);
        for (auto it = json.begin(); it != json.end(); ++it)
            result.AddMember(
                Serialize<QString>::get(it.key(), allocator),
                Serialize<QString>::get(it.value().toString(), allocator),
                allocator);
        return result;
    }
};
template <>
struct Deserialize<chatterino::satorino::TrainColours> {
    static chatterino::satorino::TrainColours get(const rapidjson::Value &value,
                                                  bool *error = nullptr)
    {
        if (!value.IsObject())
        {
            if (error)
                *error = true;
            return chatterino::satorino::defaultTrainColours();
        }
        QJsonObject json;
        for (std::size_t i = 0; i < chatterino::satorino::TRAIN_TYPE_COUNT; ++i)
        {
            const auto key = chatterino::satorino::trainTypeKey(
                static_cast<chatterino::satorino::TrainType>(i));
            const auto bytes = key.toUtf8();
            const auto field = value.FindMember(bytes.constData());
            if (field != value.MemberEnd() && field->value.IsString())
                json.insert(key, Deserialize<QString>::get(field->value));
        }
        return chatterino::satorino::trainColoursFromJson(json);
    }
};
}  // namespace pajlada
