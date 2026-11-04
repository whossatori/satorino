// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#include "controllers/autorespond/AutoRespondSettings.hpp"

#include "debug/AssertInGuiThread.hpp"

namespace chatterino::satorino {
namespace {

template <typename Rule>
void syncRows(SignalVector<Rule> &rows, const std::vector<Rule> &rules)
{
    if (rows.raw() == rules)
        return;
    rows.clear();
    for (const auto &rule : rules)
        rows.append(rule);
}

template <typename Rule, typename Changed>
void watchRows(SignalVector<Rule> &rows,
               pajlada::Signals::SignalHolder &connections, Changed changed)
{
    connections.managedConnect(rows.itemInserted, [changed](auto) {
        changed();
    });
    connections.managedConnect(rows.itemRemoved, [changed](auto) {
        changed();
    });
    connections.managedConnect(rows.itemsCleared, changed);
}

}  // namespace

AutoRespondSettings::AutoRespondSettings()
{
    this->triggers.connect(
        [this] {
            assertInGuiThread();
            this->synchronizing_ = true;
            syncRows(this->triggerRows, this->triggers.getValueCopy());
            this->synchronizing_ = false;
        },
        this->connections_);
    this->timers.connect(
        [this] {
            assertInGuiThread();
            this->synchronizing_ = true;
            syncRows(this->timerRows, this->timers.getValueCopy());
            this->synchronizing_ = false;
        },
        this->connections_);
    watchRows(this->triggerRows, this->connections_, [this] {
        if (!this->synchronizing_)
            this->triggers.setValue(this->triggerRows.raw());
    });
    watchRows(this->timerRows, this->connections_, [this] {
        if (!this->synchronizing_)
            this->timers.setValue(this->timerRows.raw());
    });
}

}  // namespace chatterino::satorino
