// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <set>

namespace chatterino::satorino {

// GUI-thread state separate from transport assignment/backlog. A websocket
// being open does not mean that its room subscription has been acknowledged.
class AutoRespondConnectionState
{
public:
    void reset()
    {
        this->confirmed_.clear();
    }
    void confirm(const QString &topic)
    {
        this->confirmed_.insert(topic);
    }
    void forget(const QString &topic)
    {
        this->confirmed_.erase(topic);
    }
    bool connected(const QString &topic, bool open, bool assigned) const
    {
        return open && assigned && this->confirmed_.contains(topic);
    }

private:
    std::set<QString> confirmed_;
};

}  // namespace chatterino::satorino
