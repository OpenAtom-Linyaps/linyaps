// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/adaptors/task/task1.h"
#include "linglong/common/dbus/register.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/formatter.h" // IWYU pragma: keep

#include <fmt/format.h>
#include <sys/prctl.h>

#include <utility>

const auto TASK_DONE = 100;

namespace linglong::service {

PackageTask::PackageTask(std::function<void(Task &)> job, QObject *parent)
    : QObject(parent)
    , Task(job)
    , m_cancelFlag(g_cancellable_new())
{
    setReporter(this);
}

PackageTask::~PackageTask()
{
    if (m_cancelFlag != nullptr) {
        g_object_unref(m_cancelFlag);
    }
}

void PackageTask::onProgress() noexcept
{
    auto taskPercentage = percentage();
    auto taskMessage = Task::message();

    LogD("task {} onProgress {} {}", taskID(), taskPercentage, taskMessage);

    Q_EMIT MessageChanged(QString::fromStdString(taskMessage));
    Q_EMIT PercentageChanged(taskPercentage);
    Q_EMIT changePropertiesDone();
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
    Q_EMIT changePropertiesDone();
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

utils::error::Result<void> PackageTask::exposeOnDBus(const QDBusConnection &connection) noexcept
{
    LINGLONG_TRACE(fmt::format("expose task {} on dbus", taskID()));

    if (m_forwarder != nullptr) {
        return LINGLONG_OK;
    }

    auto *ptr = new linglong::adaptors::task::Task1(this);
    const auto *mo = ptr->metaObject();
    auto interfaceIndex = mo->indexOfClassInfo("D-Bus Interface");
    if (interfaceIndex == -1) {
        return LINGLONG_ERR("internal adaptor error");
    }
    auto ret = common::dbus::registerDBusObject(connection, taskObjectPath().c_str(), this);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    const auto *interface = mo->classInfo(interfaceIndex).value();
    m_forwarder =
      new common::dbus::PropertiesForwarder(connection, taskObjectPath().c_str(), interface, this);

    QObject::connect(this, &PackageTask::changePropertiesDone, m_forwarder, [this]() {
        auto ret = m_forwarder->forward();
        if (!ret) {
            LogE("forward propertiesChanged failed: {}", ret.error());
        }
    });

    return LINGLONG_OK;
}

PackageTaskQueue::PackageTaskQueue(QObject *parent)
    : QObject(parent)
{
}

PackageTaskQueue::~PackageTaskQueue()
{
    if (m_taskThread.joinable()) {
        m_taskThread.join();
    }
}

// tryRunTask runs on PackageTaskQueue's thread
void PackageTaskQueue::tryRunTask()
{
    for (auto it = m_taskQueue.begin(); it != m_taskQueue.end();) {
        // remove done task
        if ((*it)->isTaskDone()) {
            LogD("task {} is done, remove it", (*it)->taskID());
            it = m_taskQueue.erase(it);
            continue;
        }

        // skip non-queued task
        if ((*it)->state() != linglong::api::types::v1::State::Queued) {
            LogW("task {} at front is not in queued state, skip it", (*it)->taskID());
            ++it;
            continue;
        }

        // std::list::iterator is valid across insert/erase
        if (m_taskThread.joinable()) {
            m_taskThread.join();
        }
        m_taskThread = std::thread([this, it]() {
            prctl(PR_SET_NAME, fmt::format("task-{}", (*it)->taskID()).c_str(), 0, 0, 0);

            LogD("task {} started", (*it)->taskID());
            (*it)->run();

            if (!(*it)->isTaskDone()) {
                LogW("task {} is not done", (*it)->taskID());
            } else {
                LogD("task {} is done", (*it)->taskID());
            }

            Q_EMIT taskDone(QString::fromStdString((*it)->taskID()));

            QMetaObject::invokeMethod(
              this,
              [this, it]() {
                  m_taskQueue.erase(it);
                  tryRunTask();
              },
              Qt::QueuedConnection);
        });

        return;
    }
}

Task &PackageTaskQueue::enqueueTask(std::unique_ptr<Task> task)
{
    LINGLONG_TRACE(fmt::format("enqueue task {}", task->taskID()));
    bool isFirst = m_taskQueue.empty();
    auto &ref = m_taskQueue.emplace_back(std::move(task));
    if (isFirst) {
        QMetaObject::invokeMethod(
          this,
          [this]() {
              tryRunTask();
          },
          Qt::QueuedConnection);
    }
    LogD("task {} enqueued", ref->taskID());
    return *ref;
}

utils::error::Result<std::reference_wrapper<Task>>
PackageTaskQueue::getTask(const std::string &taskID) noexcept
{
    LINGLONG_TRACE(fmt::format("get task {}", taskID));

    auto it = std::find_if(m_taskQueue.begin(),
                           m_taskQueue.end(),
                           [taskID](const std::unique_ptr<Task> &task) {
                               return task->taskID() == taskID;
                           });
    if (it == m_taskQueue.end()) {
        return LINGLONG_ERR(fmt::format("task {} not found", taskID));
    }
    return *it->get();
}

} // namespace linglong::service
