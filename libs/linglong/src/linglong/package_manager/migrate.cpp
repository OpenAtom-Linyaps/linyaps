// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "migrate.h"

namespace linglong::service {

Migrate::Migrate(QObject *parent)
    : QObject(parent)
{
}

void Migrate::WaitForAvailable() noexcept
{
    // there no need do anything, dbus service is blocking currently.
}

} // namespace linglong::service
