// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/package_manager/task.h"
#include "linglong/utils/dbus/properties_forwarder.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QEvent>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUuid>

#include <functional>
#include <memory>
#include <optional>
#include <thread>

Q_DECLARE_METATYPE(linglong::api::types::v1::State)

namespace linglong::service {

class PackageTaskQueue;

class PackageTask : public QObject, protected QDBusContext, public Task, public TaskReporter
{
    Q_OBJECT
public:
    Q_PROPERTY(int State READ getPropertyState NOTIFY StateChanged)
    Q_PROPERTY(QString Message READ getPropertyMessage NOTIFY MessageChanged)
    Q_PROPERTY(int Code READ getPropertyCode NOTIFY CodeChanged)
    Q_PROPERTY(double Percentage READ percentage NOTIFY PercentageChanged)

    explicit PackageTask(std::function<void(Task &)> job, QObject *parent = nullptr);
    PackageTask(PackageTask &&other) = delete;
    PackageTask &operator=(PackageTask &&other) = delete;
    ~PackageTask() override;

    void onProgress() noexcept override;
    void onStateChanged() noexcept override;

    // message report when progress or state changed
    void onMessage() noexcept override { }

    void onHandled(uint handled, uint total) noexcept override
    {
        Q_EMIT PartChanged(handled, total);
    }

    [[nodiscard]] int getPropertyState() const noexcept { return static_cast<int>(state()); }

    [[nodiscard]] QString getPropertyMessage() const noexcept
    {
        return QString::fromStdString(Task::message());
    }

    [[nodiscard]] int getPropertyCode() const noexcept { return static_cast<int>(code()); }

    [[nodiscard]] std::string taskObjectPath() const noexcept
    {
        return "/org/deepin/linglong/Task1/" + taskID();
    }

    virtual GCancellable *cancellable() noexcept override { return m_cancelFlag; }

    utils::error::Result<void> exposeOnDBus(const QDBusConnection &connection) noexcept;

public Q_SLOTS:
    void Cancel() noexcept;

Q_SIGNALS:
    void StateChanged(int newState);
    void PercentageChanged(double newPercentage);
    void MessageChanged(QString newMessage);
    void PartChanged(uint fetched, uint request);
    void CodeChanged(int newCode);

    void changePropertiesDone();

private:
    friend class PackageTaskQueue;
    GCancellable *m_cancelFlag{ nullptr };
    utils::dbus::PropertiesForwarder *m_forwarder{ nullptr };
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
    addPackageTask(Func &&job, std::optional<QDBusConnection> conn = std::nullopt) noexcept;

    template <typename Func>
    utils::error::Result<std::reference_wrapper<Task>> addTask(Func &&job) noexcept;

    utils::error::Result<std::reference_wrapper<Task>> getTask(const std::string &taskID) noexcept;

Q_SIGNALS:
    void taskDone(const QString &taskID);

private:
    Task &enqueueTask(std::unique_ptr<Task> task);
    void tryRunTask();

    std::list<std::unique_ptr<Task>> m_taskQueue;
    std::thread m_taskThread;
};

template <typename Func>
utils::error::Result<std::reference_wrapper<PackageTask>>
PackageTaskQueue::addPackageTask(Func &&job, std::optional<QDBusConnection> conn) noexcept
{
    LINGLONG_TRACE("add package task");
    static_assert(std::is_invocable_r_v<void, Func, Task &>, "mismatch function signature");

    PackageTask &task = dynamic_cast<PackageTask &>(
      enqueueTask(std::make_unique<PackageTask>(std::forward<Func>(job), this)));

    if (conn) {
        auto ret = task.exposeOnDBus(*conn);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

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
