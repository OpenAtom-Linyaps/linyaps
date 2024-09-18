// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QObject>

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

Q_SIGNALS:
    void MigrateDone(int code, QString message);
};
} // namespace linglong::service
