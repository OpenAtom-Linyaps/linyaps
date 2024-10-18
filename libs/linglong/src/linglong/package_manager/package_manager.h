/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "task.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

namespace linglong::service {

class JobQueue : public QObject
{
    Q_OBJECT
private:
    uint runningJobsCount = 0;
    uint runningJobsMax = 1;
    std::list<std::function<void()>> taskList = {};

    void run()
    {
        // 检查任务队列是否为空
        if (taskList.size() == 0) {
            runningJobsCount--;
            return;
        }
        // 从队列中取出任务
        auto func = taskList.front();
        taskList.pop_front();
        // 执行任务
        func();

        // 如果还有任务，继续执行
        if (taskList.size() > 0) {
            QMetaObject::invokeMethod(this, &JobQueue::run, Qt::QueuedConnection);
        } else {
            runningJobsCount--;
        }
    }

public:
    void runTask(std::function<void()> func)
    {
        taskList.emplace_back(func);
        if (runningJobsCount < runningJobsMax) {
            runningJobsCount++;
            QMetaObject::invokeMethod(this, &JobQueue::run, Qt::QueuedConnection);
        }
    }
};

class PackageManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManager1")
    Q_PROPERTY(QVariantMap Configuration READ getConfiguration WRITE setConfiguration)

public:
    PackageManager(linglong::repo::OSTreeRepo &repo, QObject *parent);

    ~PackageManager() override = default;
    PackageManager(const PackageManager &) = delete;
    PackageManager(PackageManager &&) = delete;
    auto operator=(const PackageManager &) -> PackageManager & = delete;
    auto operator=(PackageManager &&) -> PackageManager & = delete;
    void Update(InstallTask &taskContext,
                const package::Reference &ref,
                const package::Reference &newRef) noexcept;

public
    Q_SLOT : [[nodiscard]] auto getConfiguration() const noexcept -> QVariantMap;
    void setConfiguration(const QVariantMap &parameters) noexcept;
    auto Install(const QVariantMap &parameters) noexcept -> QVariantMap;
    void InstallRef(InstallTask &taskContext,
                    const package::Reference &ref,
                    const std::string &module) noexcept;
    auto InstallFromFile(const QDBusUnixFileDescriptor &fd, const QString &fileType) noexcept
      -> QVariantMap;
    auto Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Migrate() noexcept -> QVariantMap;
    void CancelTask(const QString &taskID) noexcept;

Q_SIGNALS:
    void TaskListChanged(QString taskID);
    void TaskChanged(QString taskID, QString percentage, QString message, int status);
    void SearchFinished(QString jobID, QVariantMap result);

private:
    QVariantMap installFromLayer(const QDBusUnixFileDescriptor &fd) noexcept;
    QVariantMap installFromUAB(const QDBusUnixFileDescriptor &fd) noexcept;
    void pullDependency(InstallTask &taskContext,
                        const api::types::v1::PackageInfoV2 &info,
                        const std::string &module) noexcept;
    linglong::repo::OSTreeRepo &repo; // NOLINT
    std::list<InstallTask> taskList;
    // 正在运行的任务ID
    QString runningTaskID;

    JobQueue m_search_queue = {};
};

} // namespace linglong::service
