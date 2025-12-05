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

    explicit PackageTask(const QDBusConnection &connection,
                         std::function<void(PackageTask &)> job,
                         QObject *parent);
    PackageTask(PackageTask &&other) = delete;
    PackageTask &operator=(PackageTask &&other) = delete;
    ~PackageTask() override;

    static PackageTask createTemporaryTask() noexcept;

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

    [[nodiscard]] std::string taskID() const noexcept { return m_taskID; }

    [[nodiscard]] std::string taskObjectPath() const noexcept
    {
        return "/org/deepin/linglong/Task1/" + taskID();
    }

    virtual GCancellable *cancellable() noexcept override { return m_cancelFlag; }

    void run() noexcept;

public Q_SLOTS:
    void Cancel() noexcept;

Q_SIGNALS:
    void StateChanged(int newState);
    void PercentageChanged(double newPercentage);
    void MessageChanged(QString newMessage);
    void PartChanged(uint fetched, uint request);
    void CodeChanged(int newCode);

private:
    friend class PackageTaskQueue;
    PackageTask();
    std::string m_taskID;
    GCancellable *m_cancelFlag{ nullptr };
    std::function<void(PackageTask &)> m_job;
    utils::dbus::PropertiesForwarder *m_forwarder{ nullptr };

    void changePropertiesDone() const noexcept;
};

class PackageTaskQueue : public QObject

{
    Q_OBJECT
public:
    explicit PackageTaskQueue(QObject *parent);

    template <typename Func>
    utils::error::Result<std::reference_wrapper<PackageTask>>
    addNewTask(Func &&job, const QDBusConnection &conn = QDBusConnection::sessionBus()) noexcept
    {
        static_assert(std::is_invocable_r_v<void, Func, PackageTask &>,
                      "mismatch function signature");

        auto &ref = m_taskQueue.emplace_back(conn, std::forward<Func>(job), this);

        Q_EMIT taskAdded();
        return ref;
    }

Q_SIGNALS:
    void taskDone(const std::string &id);
    void startTask();
    void taskAdded();

private:
    std::list<PackageTask> m_taskQueue;
};

} // namespace linglong::service
