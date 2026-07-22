// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "package_task.h"

#include "linglong/adaptors/task/task1.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/dbus/register.h"
#include "linglong/common/serialize/json.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/formatter.h" // IWYU pragma: keep

#include <fmt/format.h>
#include <sys/prctl.h>

#include <utility>

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
    const auto snapshot = stateSnapshot();
    LogD("task {} onProgress {}: {}", taskID(), snapshot.percentage, snapshot.message);
    emitStateEvent(snapshot);
}

void PackageTask::onStateChanged() noexcept
{
    const auto snapshot = stateSnapshot();
    LogD("task {} updateState {}: {}",
         taskID(),
         static_cast<int>(snapshot.state),
         snapshot.message);
    emitStateEvent(snapshot);

    if (!isDoneState(snapshot.state)) {
        return;
    }

    Q_EMIT terminalStateReached();
}

void PackageTask::onStateMessageChanged() noexcept
{
    const auto snapshot = stateSnapshot();
    LogD("task {} updateStateMessage {}", taskID(), snapshot.message);
    emitStateEvent(snapshot);
}

void PackageTask::emitStateEvent(const StateSnapshot &snapshot) noexcept
{
    Q_EMIT TaskEvent(QStringLiteral("state"),
                     common::serialize::toQVariantMap(api::types::v1::TaskState{
                       .message = snapshot.message,
                       .progress = snapshot.percentage,
                       .state = snapshot.state,
                     }));
}

void PackageTask::onMessage(const std::string &message) noexcept
{
    LogD("task {} sendMessage {}", taskID(), message);

    Q_EMIT TaskEvent(QStringLiteral("message"),
                     { { QStringLiteral("message"), QString::fromStdString(message) } });
}

void PackageTask::finish() noexcept
{
    if (m_finishedEmitted.exchange(true)) {
        return;
    }

    const auto snapshot = stateSnapshot();
    auto resultCode = snapshot.code;
    if (resultCode == utils::error::ErrorCode::Unknown) {
        switch (snapshot.state) {
        case api::types::v1::State::Succeed:
            resultCode = utils::error::ErrorCode::Success;
            break;
        case api::types::v1::State::Canceled:
            resultCode = utils::error::ErrorCode::Canceled;
            break;
        case api::types::v1::State::Failed:
            resultCode = utils::error::ErrorCode::Failed;
            break;
        default:
            break;
        }
    }

    if (snapshot.state == api::types::v1::State::Succeed && m_result) {
        if (!m_result->contains(QStringLiteral("type"))) {
            LogW("task {} result doesn't contain a type", taskID());
        }
        Q_EMIT TaskFinished(std::move(*m_result));
        return;
    }

    Q_EMIT TaskFinished(common::serialize::toQVariantMap(api::types::v1::CommonResult{
      .code = static_cast<int>(resultCode),
      .message = snapshot.message,
    }));
}

void PackageTask::Start() noexcept
{
    if (state() != api::types::v1::State::Pending) {
        return;
    }

    updateState(api::types::v1::State::Queued, Task::message());
    Q_EMIT startRequested();
}

void PackageTask::Cancel() noexcept
{
    if (isTaskDone()) {
        return;
    }

    LogI("user attempts to cancel task {}", taskID());
    auto msg = fmt::format("task {} has been canceled by user", taskID());
    updateState(linglong::api::types::v1::State::Canceled, msg);

    if (m_cancelFlag == nullptr || g_cancellable_is_cancelled(m_cancelFlag) == TRUE) {
        return;
    }

    g_cancellable_cancel(m_cancelFlag);
}

void PackageTask::setCallerContext(const CallerContext &ctx)
{
    m_callerContext = ctx;

    if (ctx.isPeerMode()) {
        const auto watchingPeer = m_callerContext.connection.connect("",
                                                                     "/org/freedesktop/DBus/Local",
                                                                     "org.freedesktop.DBus.Local",
                                                                     "Disconnected",
                                                                     this,
                                                                     SLOT(onCallerDisconnected()));
        if (!watchingPeer) {
            LogW("failed to watch peer caller for task {}", taskID());
        }
        return;
    }

    const auto callerName = ctx.callerBusName();
    if (callerName.isEmpty()) {
        return;
    }

    m_callerWatcher =
      std::make_unique<QDBusServiceWatcher>(callerName,
                                            ctx.connection,
                                            QDBusServiceWatcher::WatchForUnregistration,
                                            this);
    QObject::connect(m_callerWatcher.get(),
                     &QDBusServiceWatcher::serviceUnregistered,
                     this,
                     &PackageTask::onCallerDisconnected);
}

void PackageTask::onCallerDisconnected() noexcept
{
    if (state() != api::types::v1::State::Pending) {
        return;
    }

    LogW("caller disconnected before task {} was started", taskID());
    updateState(api::types::v1::State::Canceled, "caller disconnected");
}

utils::error::Result<void> PackageTask::exposeOnDBus() noexcept
{
    LINGLONG_TRACE(fmt::format("expose task {} on dbus", taskID()));

    if (m_exposed) {
        return LINGLONG_OK;
    }

    auto *ptr = new linglong::adaptors::task::Task1(this);
    const auto *mo = ptr->metaObject();
    auto interfaceIndex = mo->indexOfClassInfo("D-Bus Interface");
    if (interfaceIndex == -1) {
        return LINGLONG_ERR("internal adaptor error");
    }
    auto ret =
      common::dbus::registerDBusObject(m_callerContext.connection, taskObjectPath().c_str(), this);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    m_exposed = true;

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

void PackageTaskQueue::finishTask(Task &task) noexcept
{
    LINGLONG_TRACE(fmt::format("finish task {}", task.taskID()));

    if (!task.isTaskDone()) {
        task.reportError(LINGLONG_ERRV("task returned without a terminal state"));
    }

    if (auto *packageTask = dynamic_cast<PackageTask *>(&task)) {
        packageTask->finish();
    }
}

// tryRunTask runs on PackageTaskQueue's thread
void PackageTaskQueue::tryRunTask()
{
    for (auto it = m_taskQueue.begin(); it != m_taskQueue.end();) {
        if (it->get() != m_runningTask && (*it)->isTaskDone()) {
            LogD("task {} is done, remove it", (*it)->taskID());
            finishTask(**it);
            it = m_taskQueue.erase(it);
            continue;
        }
        ++it;
    }

    if (m_runningTask != nullptr) {
        return;
    }

    for (auto it = m_taskQueue.begin(); it != m_taskQueue.end(); ++it) {
        // skip non-queued task
        if ((*it)->state() != linglong::api::types::v1::State::Queued) {
            LogW("task {} at front is not in queued state, skip it", (*it)->taskID());
            continue;
        }

        // std::list::iterator remains valid when other tasks are inserted or removed.
        if (m_taskThread.joinable()) {
            m_taskThread.join();
        }
        auto taskIt = it;
        m_runningTask = taskIt->get();
        m_taskThread = std::thread([this, taskIt]() {
            auto &task = **taskIt;
            prctl(PR_SET_NAME, fmt::format("task-{}", task.taskID()).c_str(), 0, 0, 0);

            LogD("task {} started", task.taskID());
            if (!task.isTaskDone()) {
                task.run();
            }

            if (!task.isTaskDone()) {
                LogW("task {} is not done", task.taskID());
            } else {
                LogD("task {} is done", task.taskID());
            }

            QMetaObject::invokeMethod(
              this,
              [this, taskIt]() {
                  finishTask(**taskIt);
                  m_taskQueue.erase(taskIt);
                  m_runningTask = nullptr;
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
    auto &ref = m_taskQueue.emplace_back(std::move(task));
    QMetaObject::invokeMethod(
      this,
      [this]() {
          tryRunTask();
      },
      Qt::QueuedConnection);
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
