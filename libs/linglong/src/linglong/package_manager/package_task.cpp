// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/adaptors/task/task1.h"
#include "linglong/utils/dbus/register.h"

#include <QDebug>
#include <QUuid>

#include <utility>

const auto TASK_DONE = 100;

namespace linglong::service {

PackageTask PackageTask::createTemporaryTask() noexcept
{
    return {};
}

std::unique_ptr<PackageTask> PackageTask::creatSubTask(double partOfTotal) noexcept
{
    if (partOfTotal >= TASK_DONE) {
        qDebug() << "The total progress of subTask couldn't great than 100.";
        return nullptr;
    }

    struct EnableMaker : public PackageTask
    {
        using PackageTask::PackageTask;
    };

    auto subTask = std::make_unique<EnableMaker>();
    auto *subTaskPtr = subTask.get();
    connect(subTaskPtr, &PackageTask::StateChanged, [this](int newState) {
        this->setProperty("State", newState);
    });
    connect(subTaskPtr, &PackageTask::SubStateChanged, [this](int newSubState) {
        this->setProperty("SubState", newSubState);
    });
    connect(subTaskPtr, &PackageTask::MessageChanged, [this](const QString &newMessage) {
        this->setProperty("Message", newMessage);
    });
    connect(subTaskPtr, &PackageTask::PercentageChanged, [this, partOfTotal](double newPercentage) {
        this->m_totalPercentage = newPercentage * partOfTotal;
        Q_EMIT this->PercentageChanged(this->getPercentage());
    });

    return subTask;
}

PackageTask::PackageTask()
    : m_taskID(QUuid::createUuid())
    , m_cancelFlag(g_cancellable_new())
{
}

PackageTask::PackageTask(QDBusConnection connection, QStringList refs, QObject *parent)
    : QObject(parent)
    , m_taskID(QUuid::createUuid())
    , m_refs(std::move(refs))
    , m_cancelFlag(g_cancellable_new())
{
    auto *ptr = new linglong::adaptors::task::Task1(this);
    const auto *mo = ptr->metaObject();
    auto interfaceIndex = mo->indexOfClassInfo("D-Bus Interface");
    if (interfaceIndex == -1) {
        qFatal("internal adaptor error");
        return;
    }
    auto ret = linglong::utils::dbus::registerDBusObject(connection, taskObjectPath(), this);
    if (!ret) {
        qCritical() << ret.error();
        return;
    }

    const auto *interface = mo->classInfo(interfaceIndex).value();
    m_forwarder =
      new utils::dbus::PropertiesForwarder(connection, taskObjectPath(), interface, this);
}

PackageTask::~PackageTask()
{
    if (m_cancelFlag != nullptr) {
        g_object_unref(m_cancelFlag);
    }
    qDebug() << "Task: " << taskObjectPath() << "finished...";
}

void PackageTask::changePropertiesDone() const noexcept
{
    if (m_forwarder == nullptr) {
        return;
    }

    auto ret = m_forwarder->forward();
    if (!ret) {
        qCritical() << ret.error();
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

    this->setProperty("Message", message);
    m_curStagePercentage = static_cast<double>(part) / whole;
    auto partPercentageMsg =
      QString("%1/%2(%3%)")
        .arg(part)
        .arg(whole)
        .arg(m_totalPercentage
             + (m_curStagePercentage
                * m_subStateMap[static_cast<api::types::v1::SubState>(m_subState)]));

    Q_EMIT PercentageChanged(getPercentage());
    changePropertiesDone();

    Q_EMIT PartChanged(partPercentageMsg, QPrivateSignal{});
}

void PackageTask::updateState(linglong::api::types::v1::State newState,
                              const QString &message,
                              std::optional<linglong::api::types::v1::SubState> optDone) noexcept
{
    this->setProperty("State", static_cast<int>(newState));
    auto curState = state();
    // Each part is completed, count it and reset the percentage
    if (curState == linglong::api::types::v1::State::PartCompleted) {
        ++m_taskParts;
        m_totalPercentage = TASK_DONE;
        Q_EMIT PercentageChanged(m_totalPercentage);

        m_totalPercentage = 0;
        m_curStagePercentage = 0;
    }

    // Every part is completed, it means succeed
    if (this->m_taskParts == this->m_refs.size()) {
        this->setProperty("State", static_cast<int>(linglong::api::types::v1::State::Succeed));
        curState = state();
    }

    if (curState == linglong::api::types::v1::State::Canceled
        || curState == linglong::api::types::v1::State::Failed
        || curState == linglong::api::types::v1::State::Succeed) {
        auto subState = optDone.value_or(linglong::api::types::v1::SubState::AllDone);
        updateSubState(subState, message);
        return;
    }
    this->setProperty("Message", message);
    changePropertiesDone();
}

void PackageTask::updateSubState(linglong::api::types::v1::SubState newSubState,
                                 const QString &message) noexcept
{
    this->setProperty("SubState", static_cast<int>(newSubState));
    this->setProperty("Message", message);

    auto curSubState = subState();
    if (curSubState == linglong::api::types::v1::SubState::AllDone
        || curSubState == linglong::api::types::v1::SubState::PackageManagerDone) {
        m_totalPercentage = TASK_DONE;
        Q_EMIT PercentageChanged(getPercentage());
        changePropertiesDone();
        return;
    }

    m_totalPercentage += m_subStateMap[curSubState];
    m_curStagePercentage = 0;
    Q_EMIT PercentageChanged(getPercentage());
    changePropertiesDone();
}

void PackageTask::reportError(linglong::utils::error::Error &&err) noexcept
{
    m_totalPercentage = TASK_DONE;
    m_curStagePercentage = 0;
    Q_EMIT PercentageChanged(getPercentage());

    this->setProperty("State", static_cast<int>(linglong::api::types::v1::State::Failed));
    this->setProperty("SubState", static_cast<int>(linglong::api::types::v1::SubState::AllDone));
    m_err = std::move(err);

    this->setProperty("Message", m_err.message());
    changePropertiesDone();
}

void PackageTask::Cancel() noexcept
{
    if (g_cancellable_is_cancelled(m_cancelFlag) == TRUE) {
        return;
    }

    qInfo() << "task " << taskID() << "has been canceled by user";
    g_cancellable_cancel(m_cancelFlag);
    updateState(linglong::api::types::v1::State::Canceled,
                QString{ "task %1 has been canceled by user" }.arg(taskID()));
}

} // namespace linglong::service
