// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "package_task.h"

#include <QObject>
#include <QDBusContext>

namespace linglong::service {
class Task : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.Task1")
    Q_PROPERTY(quint32 State READ state)
    Q_PROPERTY(quint32 SubState READ subState)
    Q_PROPERTY(double Percentage READ percentage)

public:
    explicit Task(PackageTask *parent)
        : QObject(parent)
    {
    }

    ~Task() override = default;

    [[nodiscard]] quint32 state() const noexcept;
    [[nodiscard]] quint32 subState() const noexcept;
    [[nodiscard]] double percentage() const noexcept;

public Q_SLOTS:
    void Cancel();
};
} // namespace linglong::service
