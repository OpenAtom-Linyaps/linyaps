/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/repo/ostree_repo.h"
#include "package_task.h"

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
    std::list<std::function<void()>> taskList;

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

    ~PackageManager() override;
    PackageManager(const PackageManager &) = delete;
    PackageManager(PackageManager &&) = delete;
    auto operator=(const PackageManager &) -> PackageManager & = delete;
    auto operator=(PackageManager &&) -> PackageManager & = delete;
    void Update(PackageTask &taskContext,
                const package::Reference &ref,
                const package::Reference &newRef,
                const std::string &module) noexcept;

public
    Q_SLOT : [[nodiscard]] auto getConfiguration() const noexcept -> QVariantMap;
    void setConfiguration(const QVariantMap &parameters) noexcept;
    auto Install(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto InstallFromFile(const QDBusUnixFileDescriptor &fd,
                         const QString &fileType,
                         const QVariantMap &options) noexcept -> QVariantMap;
    auto Uninstall(const QVariantMap &parameters) noexcept -> QVariantMap;
    void Uninstall(PackageTask &taskContext,
                   const package::Reference &ref,
                   const std::string &module) noexcept;
    auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Migrate() noexcept -> QVariantMap;
    auto Prune() noexcept -> QVariantMap;
    utils::error::Result<void>
    Prune(std::vector<api::types::v1::PackageInfoV2> &removedInfo) noexcept;
    void ReplyInteraction(QDBusObjectPath object_path, const QVariantMap &replies);

Q_SIGNALS:
    void TaskAdded(QDBusObjectPath object_path);
    void TaskRemoved(
      QDBusObjectPath object_path, int state, int subState, QString message, double percentage);
    void TaskListChanged(const QString &taskObjectPath);
    void RequestInteraction(QDBusObjectPath object_path,
                            int messageID,
                            QVariantMap additionalMessage);
    void SearchFinished(QString jobID, QVariantMap result);
    void PruneFinished(QString jobID, QVariantMap result);
    void ReplyReceived(const QVariantMap &replies);

private:
    void Install(PackageTask &taskContext,
                 const package::Reference &newRef,
                 std::optional<package::Reference> oldRef,
                 const std::string &module) noexcept;
    void InstallRef(PackageTask &taskContext,
                    const package::Reference &ref,
                    const std::string &module) noexcept;
    void UninstallRef(PackageTask &taskContext,
                      const package::Reference &ref,
                      const std::string &module) noexcept;
    QVariantMap installFromLayer(const QDBusUnixFileDescriptor &fd,
                                 const api::types::v1::CommonOptions &options) noexcept;
    QVariantMap installFromUAB(const QDBusUnixFileDescriptor &fd,
                               const api::types::v1::CommonOptions &options) noexcept;
    void pullDependency(PackageTask &taskContext,
                        const api::types::v1::PackageInfoV2 &info,
                        const std::string &module) noexcept;
    [[nodiscard]] utils::error::Result<void> lockRepo() noexcept;
    [[nodiscard]] utils::error::Result<void> unlockRepo() noexcept;
    [[nodiscard]] static utils::error::Result<
      std::vector<api::types::v1::ContainerProcessStateInfo>>
    getAllRunningContainers() noexcept;
    utils::error::Result<bool> isRefBusy(const package::Reference &ref) noexcept;
    void deferredUninstall() noexcept;
    utils::error::Result<void> removeAfterInstall(const package::Reference &oldRef) noexcept;
    linglong::repo::OSTreeRepo &repo; // NOLINT
    std::list<PackageTask *> taskList;
    // 正在运行的任务对象路径
    QString runningTaskObjectPath;

    JobQueue m_search_queue = {};
    JobQueue m_prune_queue = {};

    int lockFd{ -1 };
};

} // namespace linglong::service
