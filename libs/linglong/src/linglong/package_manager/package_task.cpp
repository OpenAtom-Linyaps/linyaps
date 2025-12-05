// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/adaptors/task/task1.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/log/formatter.h" // IWYU pragma: keep

#include <fmt/format.h>

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
    : m_taskID(QUuid::createUuid().toString(QUuid::Id128).toStdString())
    , m_cancelFlag(g_cancellable_new())
{
    setReporter(this);
    setState(linglong::api::types::v1::State::Processing);
    LogD("task created: {}", m_taskID);
}

PackageTask::PackageTask(const QDBusConnection &connection,
                         std::function<void(PackageTask &)> job,
                         QObject *parent)
    : QObject(parent)
    , m_taskID(QUuid::createUuid().toString(QUuid::Id128).toStdString())
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
    auto ret =
      linglong::utils::dbus::registerDBusObject(connection, taskObjectPath().c_str(), this);
    if (!ret) {
        qCritical() << ret.error();
        return;
    }

    const auto *interface = mo->classInfo(interfaceIndex).value();
    m_forwarder =
      new utils::dbus::PropertiesForwarder(connection, taskObjectPath().c_str(), interface, this);

    setReporter(this);
    LogD("task {} created on dbus", m_taskID);
}

PackageTask::~PackageTask()
{
    if (m_cancelFlag != nullptr) {
        g_object_unref(m_cancelFlag);
    }
    LogD("task {} finished", taskID());
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

void PackageTask::onProgress() noexcept
{
    auto taskPercentage = percentage();
    auto taskMessage = Task::message();

    LogD("task {} onProgress {} {}", taskID(), taskPercentage, taskMessage);

    Q_EMIT MessageChanged(QString::fromStdString(taskMessage));
    Q_EMIT PercentageChanged(taskPercentage);
    changePropertiesDone();
}

void PackageTask::onStateChanged() noexcept
{
    auto taskState = static_cast<int>(state());
    auto taskMessage = Task::message();
    auto taskCode = static_cast<int>(code());

    LogD("task {} updateState {} {}", taskID(), taskState, taskMessage);

    Q_EMIT StateChanged(taskState);
    Q_EMIT MessageChanged(QString::fromStdString(taskMessage));
    Q_EMIT CodeChanged(taskCode);
    changePropertiesDone();
}

void PackageTask::Cancel() noexcept
{
    if (isTaskDone()) {
        return;
    }

    auto msg = fmt::format("task {} has been canceled by user", taskID());
    LogI(msg);
    updateState(linglong::api::types::v1::State::Canceled, msg);

    if (m_cancelFlag == nullptr || g_cancellable_is_cancelled(m_cancelFlag) == TRUE) {
        return;
    }

    g_cancellable_cancel(m_cancelFlag);
}

void PackageTask::run() noexcept
{
    m_job(*this);
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

              auto &task = m_taskQueue.front();
              if (task.state() != linglong::api::types::v1::State::Queued) {
                  qDebug() << "other task is running, wait for it done";
                  return;
              }

              task.run();
              Q_EMIT taskDone(task.taskID());
          },
          Qt::QueuedConnection);
    });

    connect(this, &PackageTaskQueue::taskDone, [this](const std::string &taskID) {
        auto task =
          std::find_if(m_taskQueue.begin(), m_taskQueue.end(), [&taskID](const auto &task) {
              return task.taskID() == taskID;
          });

        if (task == m_taskQueue.end()) {
            LogE("task {} not found", taskID);
            return;
        }

        // if queued task is done, only remove it from queue
        // otherwise, remove it and start next task
        bool isQueuedDone = task->state() == linglong::api::types::v1::State::Queued;
        Q_EMIT qobject_cast<PackageManager *>(this->parent())
          ->TaskRemoved(QDBusObjectPath{ task->taskObjectPath().c_str() });
        m_taskQueue.erase(task);

        if (!isQueuedDone) {
            Q_EMIT startTask();
        }
    });
}

} // namespace linglong::service
