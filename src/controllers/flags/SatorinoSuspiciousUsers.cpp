// SPDX-FileCopyrightText: 2026 whossatori <satoriyoxi@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/flags/SatorinoSuspiciousUsers.hpp"
#include "debug/AssertInGuiThread.hpp"

namespace chatterino::satorino {
SuspiciousUserCache &nativeSuspiciousUsers()
{
    assertInGuiThread();
    static SuspiciousUserCache cache;
    return cache;
}
pajlada::Signals::Signal<QString, QString> &nativeSuspiciousUsersChanged()
{
    assertInGuiThread();
    static pajlada::Signals::Signal<QString, QString> changed;
    return changed;
}
void notifyNativeSuspiciousUser(const QString &room, const QString &user)
{
    nativeSuspiciousUsersChanged().invoke(room, user);
}
void observeNativeSuspiciousUser(const QString &room, const QString &user,
                                 SuspiciousState state, qint64 atMs)
{
    if (nativeSuspiciousUsers().observe(room, user, state, atMs))
        notifyNativeSuspiciousUser(room, user);
}
}  // namespace chatterino::satorino
