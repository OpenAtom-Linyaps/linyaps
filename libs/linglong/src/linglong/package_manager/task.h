// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "package_task.h"

#include <QDBusContext>

namespace linglong::service {
class Task : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.Task1")
    Q_PROPERTY(QString State READ state)
    Q_PROPERTY(QString SubState READ subState)

public:
    explicit Task(PackageTask *parent)
        : QObject(parent)
    {
    }

    [[nodiscard]] QString state() const noexcept;
    [[nodiscard]] QString subState() const noexcept;

public Q_SLOTS:
    void Cancel();
};
} // namespace linglong::service
