// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/adaptors/task/task1.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/global/initialize.h"

#include <QDebug>
#include <QUuid>

#include <utility>

const auto TASK_DONE = 100;

namespace linglong::service {

PackageTask PackageTask::createTemporaryTask() noexcept
{
    return {};
}

PackageTask::PackageTask()
    : m_state(static_cast<int>(linglong::api::types::v1::State::Processing))
    , m_taskID(QUuid::createUuid())
    , m_cancelFlag(g_cancellable_new())
{
    connect(utils::global::GlobalTaskControl::instance(),
            &utils::global::GlobalTaskControl::OnCancel,
            this,
            &PackageTask::Cancel);
}

PackageTask::PackageTask(const QDBusConnection &connection,
                         QStringList refs,
                         std::function<void(PackageTask &)> job,
                         QObject *parent)
    : QObject(parent)
    , m_taskID(QUuid::createUuid())
    , m_refs(std::move(refs))
    , m_cancelFlag(g_cancellable_new())
    , m_job(std::move(job))
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

    connect(utils::global::GlobalTaskControl::instance(),
            &utils::global::GlobalTaskControl::OnCancel,
            this,
            &PackageTask::Cancel);
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

    Q_EMIT PercentageChanged(getPercentage());
    changePropertiesDone();
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

    const auto &id = taskID();
    auto oldState = state();
    updateState(linglong::api::types::v1::State::Canceled,
                QString{ "task %1 has been canceled by user" }.arg(id));
    if (oldState == linglong::api::types::v1::State::Queued) {
        auto *ptr = parent();
        if (ptr == nullptr) { // temporary task
            return;
        }

        Q_EMIT qobject_cast<PackageTaskQueue *>(ptr)->taskDone(id);
    }
}

utils::error::Result<void> PackageTask::run() noexcept
{
    LINGLONG_TRACE("run task");
    if (m_state != static_cast<int>(linglong::api::types::v1::State::Queued)) {
        qInfo() << "task" << taskID() << " is not in queued state" << static_cast<int>(state());
        return LINGLONG_ERR("task is not in queued state");
    }

    m_job(*this);
    return LINGLONG_OK;
}

PackageTaskQueue::PackageTaskQueue(QObject *parent)
    : QObject(parent)
{
    connect(this, &PackageTaskQueue::taskAdded, &PackageTaskQueue::startTask);
    connect(this, &PackageTaskQueue::startTask, [this]() {
        QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [this]() {
              if (m_taskQueue.empty()) {
                  return;
              }

              if (m_taskQueue.size() > 1) {
                  qDebug() << "other task is running, wait for it done";
                  return;
              }

              auto &task = m_taskQueue.front();
              auto ret = task.run();
              if (!ret) {
                  qWarning() << ret.error();
              }

              Q_EMIT taskDone(task.taskID());
          },
          Qt::QueuedConnection);
    });

    connect(this, &PackageTaskQueue::taskDone, [this](const QString &taskID) {
        auto task =
          std::find_if(m_taskQueue.begin(), m_taskQueue.end(), [&taskID](const auto &task) {
              return task.taskID() == taskID;
          });

        if (task == m_taskQueue.end()) {
            qCritical() << "task " << taskID << " not found";
            return;
        }

        // if queued task is done, only remove it from queue
        // otherwise, remove it and start next task
        bool isQueuedDone = task == m_taskQueue.begin();
        Q_EMIT qobject_cast<PackageManager *>(this->parent())
          ->TaskRemoved(QDBusObjectPath{ task->taskObjectPath() },
                        static_cast<int>(task->state()),
                        static_cast<int>(task->subState()),
                        task->message(),
                        task->getPercentage());
        m_taskQueue.erase(task);

        if (isQueuedDone) {
            Q_EMIT startTask();
        }
    });
}

} // namespace linglong::service
