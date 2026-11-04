// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondSettings.hpp"
#include "debug/AssertInGuiThread.hpp"
#include <QScopedValueRollback>
#include <QUuid>
#include <set>

namespace chatterino::satorino {
AutoRespondSettings::AutoRespondSettings()
{
    assertInGuiThread();
    auto manager = pajlada::Settings::SettingManager::getInstance();
    if (!manager->get("/satorino/autorespond/rules"))
    {
        auto migrated = readAutoRespondRules(
            nullptr, manager->get("/satorino/autorespond/triggers"),
            manager->get("/satorino/autorespond/timers"));
        // Persist even an empty migration so legacy lists are read only once.
        rapidjson::Document document;
        auto encoded = pajlada::Serialize<std::vector<AutoRespondRule>>::get(
            migrated, document.GetAllocator());
        manager->set("/satorino/autorespond/rules", encoded);
    }
    // Missing/duplicate IDs in hand-edited JSON must not alias cooldowns/dates.
    auto stored = this->rules.getValueCopy();
    std::set<QString> ids;
    for (auto &rule : stored)
    {
        if (rule.id.isEmpty() || !ids.insert(rule.id).second)
        {
            rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            ids.insert(rule.id);
        }
    }
    this->rules.setValue(stored);
    this->rules.connect(
        [this] {
            assertInGuiThread();
            {
                const QScopedValueRollback guard(this->synchronizing_, true);
                const auto replacement = this->rules.getValueCopy();
                if (this->rows.raw() != replacement)
                {
                    this->rows.clear();
                    for (const auto &rule : replacement)
                        this->rows.append(rule);
                }
            }
            // Models defer structural reloads until the current editor/signal returns.
            this->changed.invoke();
        },
        this->connections_);
    const auto persist = [this] {
        if (!this->synchronizing_)
            this->rules.setValue(this->rows.raw());
    };
    this->connections_.managedConnect(this->rows.itemInserted, [persist](auto) {
        persist();
    });
    this->connections_.managedConnect(this->rows.itemRemoved, [persist](auto) {
        persist();
    });
    this->connections_.managedConnect(this->rows.itemsCleared, persist);
}
}  // namespace chatterino::satorino
