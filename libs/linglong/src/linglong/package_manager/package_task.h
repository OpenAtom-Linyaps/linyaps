// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/package_manager/task.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QEvent>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

Q_DECLARE_METATYPE(linglong::api::types::v1::State)

namespace linglong::service {

struct CallerContext
{
    QDBusConnection connection{ QDBusConnection::systemBus() };
    QDBusMessage message;

    [[nodiscard]] QString callerBusName() const { return message.service(); }

    [[nodiscard]] bool isPeerMode() const { return connection.baseService().isEmpty(); }
};

class PackageTaskQueue;

class PackageTask : public QObject, protected QDBusContext, public Task, public TaskReporter
{
    Q_OBJECT
public:
    explicit PackageTask(std::function<void(Task &)> job, QObject *parent = nullptr);
    PackageTask(PackageTask &&other) = delete;
    PackageTask &operator=(PackageTask &&other) = delete;
    ~PackageTask() override;

    void onProgress() noexcept override;
    void onStateChanged() noexcept override;
    void onStateMessageChanged() noexcept override;

    // report a standalone text output event
    void onMessage(const std::string &message) noexcept override;

    void onDataArrived(uint arrived) noexcept override { Q_EMIT DataArrived(arrived); }

    void onHandled(uint handled, uint total) noexcept override
    {
        Q_EMIT PartChanged(handled, total);
    }

    [[nodiscard]] std::string taskObjectPath() const noexcept
    {
        return "/org/deepin/linglong/Task1/" + taskID();
    }

    virtual GCancellable *cancellable() noexcept override { return m_cancelFlag; }

    utils::error::Result<void> exposeOnDBus() noexcept;

    void setCallerContext(const CallerContext &ctx);

    [[nodiscard]] const CallerContext &callerContext() const noexcept { return m_callerContext; }

    // The result must contain a "type" field identifying its concrete API type.
    void setResult(QVariantMap result) noexcept { m_result = std::move(result); }

public Q_SLOTS:
    void Start() noexcept;
    void Cancel() noexcept;

Q_SIGNALS:
    void TaskEvent(QString event, QVariantMap data);
    void TaskFinished(QVariantMap result);
    void DataArrived(uint arrived);
    void PartChanged(uint fetched, uint request);
    void startRequested();
    void terminalStateReached();

private Q_SLOTS:
    void onCallerDisconnected() noexcept;

private:
    friend class PackageTaskQueue;

    void emitStateEvent(const StateSnapshot &snapshot) noexcept;
    void finish() noexcept;

    GCancellable *m_cancelFlag{ nullptr };
    CallerContext m_callerContext;
    std::unique_ptr<QDBusServiceWatcher> m_callerWatcher;
    std::atomic_bool m_finishedEmitted{ false };
    bool m_exposed{ false };
    std::optional<QVariantMap> m_result;
};

// PackageTaskQueue is used to manage tasks and run them in a separated thread
// however, the queue itself is not thread-safe and must be used from a single thread
class PackageTaskQueue : public QObject

{
    Q_OBJECT
public:
    explicit PackageTaskQueue(QObject *parent);
    ~PackageTaskQueue();

    template <typename Func>
    utils::error::Result<std::reference_wrapper<PackageTask>>
    addPackageTask(Func &&job, std::optional<CallerContext> ctx = std::nullopt) noexcept;

    template <typename Func>
    utils::error::Result<std::reference_wrapper<Task>> addTask(Func &&job) noexcept;

    utils::error::Result<std::reference_wrapper<Task>> getTask(const std::string &taskID) noexcept;

private:
    Task &enqueueTask(std::unique_ptr<Task> task);
    void finishTask(Task &task) noexcept;
    void tryRunTask();

    std::list<std::unique_ptr<Task>> m_taskQueue;
    std::thread m_taskThread;
    Task *m_runningTask{ nullptr };
};

template <typename Func>
utils::error::Result<std::reference_wrapper<PackageTask>>
PackageTaskQueue::addPackageTask(Func &&job, std::optional<CallerContext> ctx) noexcept
{
    LINGLONG_TRACE("add package task");
    static_assert(std::is_invocable_r_v<void, Func, Task &>, "mismatch function signature");

    auto ownedTask = std::make_unique<PackageTask>(std::forward<Func>(job), this);
    PackageTask &task = *ownedTask;

    if (ctx) {
        task.setState(api::types::v1::State::Pending);
        task.setCallerContext(*ctx);
        auto ret = task.exposeOnDBus();
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    enqueueTask(std::move(ownedTask));
    QObject::connect(&task,
                     &PackageTask::startRequested,
                     this,
                     &PackageTaskQueue::tryRunTask,
                     Qt::QueuedConnection);
    QObject::connect(&task,
                     &PackageTask::terminalStateReached,
                     this,
                     &PackageTaskQueue::tryRunTask,
                     Qt::QueuedConnection);

    return task;
}

template <typename Func>
utils::error::Result<std::reference_wrapper<Task>> PackageTaskQueue::addTask(Func &&job) noexcept
{
    LINGLONG_TRACE("add task");
    static_assert(std::is_invocable_r_v<void, Func, Task &>, "mismatch function signature");

    auto &task = enqueueTask(std::make_unique<Task>(std::forward<Func>(job)));

    return task;
}

} // namespace linglong::service
