// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "task.h"

#include "linglong/utils/error/error.h"

#include <QDebug>
#include <QUuid>

namespace linglong::service {

InstallTask::InstallTask(const QUuid &taskID, QObject *parent)
    : QObject(parent)
    , m_taskID(taskID)
    , m_cancelFlag(g_cancellable_new())
{
}

InstallTask::~InstallTask()
{
    g_object_unref(m_cancelFlag);
}

void InstallTask::updateTask(double currentPercentage,
                             double totalPercentage,
                             const QString &message) noexcept
{
    if (totalPercentage == 0) {
        return;
    }

    auto increase = (currentPercentage / totalPercentage) * partsMap[m_status];
    Q_EMIT TaskChanged(taskID(), formatPercentage(increase), message, m_status, {});
}

void InstallTask::updateStatus(Status newStatus, const QString &message) noexcept
{
    qInfo() << "update task" << m_taskID << "status to" << newStatus << message;

    if (newStatus == Success || newStatus == Failed || newStatus == Canceled) {
        m_statePercentage = 100;
    } else {
        m_statePercentage += partsMap[m_status];
    }

    m_status = newStatus;
    Q_EMIT TaskChanged(taskID(), formatPercentage(), message, m_status, {});
}

void InstallTask::updateStatus(Status newStatus, linglong::utils::error::Result<void> err) noexcept
{
    qInfo() << "update task" << m_taskID << "status to" << newStatus << err.error().message();

    if (newStatus == Success || newStatus == Failed || newStatus == Canceled) {
        m_statePercentage = 100;
    } else {
        m_statePercentage += partsMap[m_status];
    }

    m_status = newStatus;
    m_err.reset(&err);
    Q_EMIT TaskChanged(taskID(), formatPercentage(), err.error().message(), m_status, {});
}

QString InstallTask::formatPercentage(double increase) const noexcept
{
    QString ret;
    ret.setNum(m_statePercentage + increase, 'g', 4);
    return ret;
}

void InstallTask::cancelTask() noexcept
{
    if (g_cancellable_is_cancelled(m_cancelFlag) == 0) {
        g_cancellable_cancel(m_cancelFlag);
    }
}

} // namespace linglong::service
