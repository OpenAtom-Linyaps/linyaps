// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/serialize/json.h"

#include <QDebug>
#include <QUuid>

namespace linglong::service {

PackageTask PackageTask::createTemporaryTask() noexcept
{
    return {};
}

PackageTask::PackageTask()
    : m_taskID(QUuid::createUuid())
    , m_cancelFlag(g_cancellable_new())
{
}

PackageTask::PackageTask(const package::Reference &ref, const QString &module, QObject *parent)
    : QObject(parent)
    , m_taskID(QUuid::createUuid())
    , m_layer(ref.toString() % "/" % module)
    , m_cancelFlag(g_cancellable_new())
{
}

PackageTask::PackageTask(const package::Reference &ref, const std::string &module, QObject *parent)
    : PackageTask(ref, QString::fromStdString(module), parent)
{
}

PackageTask::PackageTask(PackageTask &&other) noexcept
    : m_state(other.m_state)
    , m_subState(other.m_subState)
    , m_err(std::move(other).m_err)
    , m_curPercentage(other.m_curPercentage)
    , m_taskID(std::move(other).m_taskID)
    , m_layer(std::move(other).m_layer)
    , m_cancelFlag(other.m_cancelFlag)
    , m_job(std::move(other).m_job)
{
    other.m_cancelFlag = nullptr;
    other.m_state = State::Unknown;
    other.m_subState = SubState::Unknown;
    other.m_curPercentage = 0;
}

PackageTask &PackageTask::operator=(PackageTask &&other) noexcept
{
    if (*this == other) {
        return *this;
    }

    this->m_state = other.m_state;
    other.m_state = State::Unknown;

    this->m_subState = other.m_subState;
    other.m_subState = SubState::Unknown;

    this->m_cancelFlag = other.m_cancelFlag;
    other.m_cancelFlag = nullptr;

    this->m_curPercentage = other.m_curPercentage;
    other.m_curPercentage = 100;

    this->m_layer = std::move(other).m_layer;
    this->m_err = std::move(other).m_err;
    this->m_taskID = std::move(other).m_taskID;
    this->m_job = std::move(other).m_job;

    return *this;
}

PackageTask::~PackageTask()
{
    if (m_cancelFlag != nullptr) {
        g_object_unref(m_cancelFlag);
    }
}

void PackageTask::updateTask(uint part, uint whole, const QString &message) noexcept
{
    if (whole == 0) {
        qWarning() << "divisor equals to zero, subState wouldn't be update";
        return;
    }

    if (part > whole) {
        qWarning() << "part is great than whole, subState wouldn't be update";
        return;
    }

    auto partPercentage = part / whole;
    auto partPercentageMsg =
      QString("%1/%2(%3%)").arg(part).arg(whole).arg(currentPercentage(partPercentage * 100));

    Q_EMIT PartChanged(partPercentageMsg, {});
    Q_EMIT TaskChanged(taskObjectPath(), message, {});
}

void PackageTask::updateState(State newState, const QString &message) noexcept
{
    m_state = newState;

    if (m_state == State::Canceled || m_state == State::Failed || m_state == State::Succeed) {
        updateSubState(SubState::Done, message);
        return;
    }

    Q_EMIT TaskChanged(taskObjectPath(), message, {});
}

void PackageTask::updateSubState(SubState newSubState, const QString &message) noexcept
{
    m_subState = newSubState;

    if (m_subState == SubState::Done) {
        m_curPercentage = 100;
    } else {
        m_curPercentage += m_subStateMap[m_subState];
    }

    Q_EMIT TaskChanged(taskObjectPath(), message, {});
}

void PackageTask::reportError(linglong::utils::error::Error &&err) noexcept
{
    m_curPercentage = 100;
    m_state = State::Failed;
    m_subState = SubState::Done;
    m_err = std::move(err);

    Q_EMIT TaskChanged(taskObjectPath(), m_err.message(), {});
}

double PackageTask::currentPercentage(double increase) noexcept
{
    return m_curPercentage + increase;
}

void PackageTask::cancelTask() noexcept
{
    if (g_cancellable_is_cancelled(m_cancelFlag) == 0) {
        g_cancellable_cancel(m_cancelFlag);
    }

    m_state = State::Canceled;
    m_subState = SubState::Done;
    
    Q_EMIT TaskChanged(taskObjectPath(), "Task canceled", {});
}

} // namespace linglong::service
