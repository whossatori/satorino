// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SatorinoGames.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <rapidjson/document.h>

#include <initializer_list>

namespace pajlada {
// Persist an actual JSON array, not JSON embedded in a string setting.
template <>
struct Serialize<chatterino::satorino::GameList> {
    static rapidjson::Value get(const chatterino::satorino::GameList &games,
                                rapidjson::Document::AllocatorType &a)
    {
        rapidjson::Value result(rapidjson::kArrayType);
        for (const auto &item : chatterino::satorino::gamesToJson(games))
        {
            rapidjson::Value obj(rapidjson::kObjectType);
            const auto fields = item.toObject();
            for (auto it = fields.begin(); it != fields.end(); ++it)
                obj.AddMember(Serialize<QString>::get(it.key(), a),
                              Serialize<QString>::get(it.value().toString(), a),
                              a);
            result.PushBack(obj, a);
        }
        return result;
    }
};
template <>
struct Deserialize<chatterino::satorino::GameList> {
    static chatterino::satorino::GameList get(const rapidjson::Value &value,
                                              bool *error = nullptr)
    {
        if (!value.IsArray())
        {
            if (error)
                *error = true;
            return {};
        }
        QJsonArray items;
        // Bound malformed/hand-edited input before building a second representation.
        for (const auto &v : value.GetArray())
        {
            if (items.size() >= 1000)
                break;
            if (!v.IsObject())
                continue;
            QJsonObject item;
            for (const auto *key : {"id", "name", "boxArtUrl", "colour"})
            {
                const auto it = v.FindMember(key);
                if (it != v.MemberEnd() && it->value.IsString())
                    item.insert(key, Deserialize<QString>::get(it->value));
            }
            items.push_back(item);
        }
        return chatterino::satorino::gamesFromJson(items);
    }
};
}  // namespace pajlada
