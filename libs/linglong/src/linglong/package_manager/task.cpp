// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "task.h"

#include <QDebug>
#include <QUuid>

namespace linglong::service {

InstallTask InstallTask::createTemporaryTask() noexcept
{
    return {};
}

InstallTask::InstallTask()
    : m_taskID(QUuid::createUuid())
    , m_cancelFlag(g_cancellable_new())
{
}

InstallTask::InstallTask(const package::Reference &ref, const QString &module, QObject *parent)
    : QObject(parent)
    , m_taskID(QUuid::createUuid())
    , m_layer(ref.toString() % "-" % module)
    , m_cancelFlag(g_cancellable_new())
{
}

InstallTask::InstallTask(const package::Reference &ref, const std::string &module, QObject *parent)
    : InstallTask(ref, QString::fromStdString(module), parent)
{
}

InstallTask::InstallTask(InstallTask &&other) noexcept
    : m_status(other.m_status)
    , m_err(std::move(other).m_err)
    , m_statePercentage(other.m_statePercentage)
    , m_taskID(std::move(other).m_taskID)
    , m_layer(std::move(other).m_layer)
    , m_cancelFlag(other.m_cancelFlag)
{
    other.m_cancelFlag = nullptr;
    other.m_status = Status::Queued;
    other.m_statePercentage = 0;
}

InstallTask &InstallTask::operator=(InstallTask &&other) noexcept
{
    if (*this == other) {
        return *this;
    }

    this->m_status = other.m_status;
    other.m_status = Status::Queued;

    this->m_cancelFlag = other.m_cancelFlag;
    other.m_cancelFlag = nullptr;

    this->m_statePercentage = other.m_statePercentage;
    other.m_statePercentage = 0;

    this->m_layer = std::move(other).m_layer;
    this->m_err = std::move(other).m_err;
    this->m_taskID = std::move(other).m_taskID;

    return *this;
}

InstallTask::~InstallTask()
{
    if (m_cancelFlag != nullptr) {
        g_object_unref(m_cancelFlag);
    }
}

void InstallTask::updateTask(double currentPercentage,
                             double totalPercentage,
                             const QString &message) noexcept
{
    if (totalPercentage == 0) {
        return;
    }
    auto increase = (currentPercentage / totalPercentage) * partsMap[Success];
    auto partPercentage = QString("%1/%2(%3%)")
                            .arg(currentPercentage)
                            .arg(totalPercentage)
                            .arg(formatPercentage(currentPercentage / totalPercentage * 100));
    Q_EMIT PartChanged(taskID(), partPercentage, message, m_status, {});
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

void InstallTask::updateStatus(Status newStatus, linglong::utils::error::Error err) noexcept
{
    qInfo() << "update task" << m_taskID << "status to" << newStatus << err.message();

    if (newStatus == Success || newStatus == Failed || newStatus == Canceled) {
        m_statePercentage = 100;
    } else {
        m_statePercentage += partsMap[m_status];
    }

    Q_EMIT TaskChanged(taskID(), formatPercentage(), err.message(), newStatus, {});
    m_status = newStatus;
    m_err = std::move(err);
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
