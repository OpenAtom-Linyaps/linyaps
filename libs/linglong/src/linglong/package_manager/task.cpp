// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "task.h"

namespace linglong::service {

void Task::Cancel()
{
    dynamic_cast<PackageTask *>(parent())->cancelTask();
}

quint32 Task::state() const noexcept
{
    auto state = dynamic_cast<PackageTask *>(parent())->state();
    return static_cast<quint32>(state);
}

quint32 Task::subState() const noexcept
{
    auto subState = dynamic_cast<PackageTask *>(parent())->subState();
    return static_cast<quint32>(subState);
}

double Task::percentage() const noexcept
{
    return dynamic_cast<PackageTask *>(parent())->currentPercentage();
}

} // namespace linglong::service
