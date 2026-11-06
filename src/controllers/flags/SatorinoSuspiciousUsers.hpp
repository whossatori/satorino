// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once
#include "util/SatorinoSuspiciousUsers.hpp"
#include <pajlada/signals/signal.hpp>

namespace chatterino::satorino {
// GUI-only shared cache and in-flight gate; observing never performs network I/O.
SuspiciousUserCache &nativeSuspiciousUsers();
pajlada::Signals::Signal<QString, QString> &nativeSuspiciousUsersChanged();
void observeNativeSuspiciousUser(const QString &room, const QString &user,
                                 SuspiciousState state, qint64 atMs);
void notifyNativeSuspiciousUser(const QString &room, const QString &user);
}  // namespace chatterino::satorino
