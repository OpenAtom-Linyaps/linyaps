// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/dbus/v1/migrate.h"

namespace linglong::service {
class Migrate : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.Migrate1")
public:
    explicit Migrate(QObject *parent);
    ~Migrate() override = default;
    Migrate(const Migrate &) = delete;
    Migrate(Migrate &&) = delete;
    Migrate &operator=(const Migrate &) = delete;
    Migrate operator=(Migrate &&) = delete;

public
    Q_SLOT : void WaitForAvailable() noexcept;
};
} // namespace linglong::service
