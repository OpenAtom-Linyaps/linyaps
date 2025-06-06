/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "package_task.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

#include <optional>

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
    PackageManager(linglong::repo::OSTreeRepo &repo,
                   linglong::runtime::ContainerBuilder &containerBuilder,
                   QObject *parent);

    ~PackageManager() override;
    PackageManager(const PackageManager &) = delete;
    PackageManager(PackageManager &&) = delete;
    auto operator=(const PackageManager &) -> PackageManager & = delete;
    auto operator=(PackageManager &&) -> PackageManager & = delete;

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
    auto Prune() noexcept -> QVariantMap;
    auto GenerateCache(const QString &reference) noexcept -> QVariantMap;
    void ReplyInteraction(QDBusObjectPath object_path, const QVariantMap &replies);

    // Nothing to do here, Permissions() will be rejected in org.deepin.linglong.PackageManager.conf
    void Permissions() { }

Q_SIGNALS:
    void TaskAdded(QDBusObjectPath object_path);
    void TaskRemoved(QDBusObjectPath object_path,
                     int state,
                     int subState,
                     QString message,
                     double percentage,
                     int code);
    void TaskListChanged(const QString &taskObjectPath, const QString &taskDescription);
    void RequestInteraction(QDBusObjectPath object_path,
                            int messageID,
                            QVariantMap additionalMessage);
    void SearchFinished(QString jobID, QVariantMap result);
    void PruneFinished(QString jobID, QVariantMap result);
    void GenerateCacheFinished(QString jobID, bool status);
    void ReplyReceived(const QVariantMap &replies);

private:
    // passing multiple modules to install may use in the future
    void Install(PackageTask &taskContext,
                 const package::Reference &newRef,
                 std::optional<package::Reference> oldRef,
                 const std::vector<std::string> &modules,
                 const std::optional<api::types::v1::Repo> &repo) noexcept;
    void InstallRef(PackageTask &taskContext,
                    const package::Reference &ref,
                    std::vector<std::string> modules,
                    const api::types::v1::Repo &repo) noexcept;
    void Update(PackageTask &taskContext,
                const package::Reference &ref,
                const package::ReferenceWithRepo &newRef) noexcept;
    void UninstallRef(PackageTask &taskContext,
                      const package::Reference &ref,
                      const std::vector<std::string> &modules) noexcept;
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
    utils::error::Result<void> removeAfterInstall(const package::Reference &oldRef,
                                                  const package::Reference &newRef,
                                                  const std::vector<std::string> &modules) noexcept;
    utils::error::Result<void>
    Prune(std::vector<api::types::v1::PackageInfoV2> &removedInfo) noexcept;
    utils::error::Result<void> generateCache(const package::Reference &ref) noexcept;
    utils::error::Result<void> tryGenerateCache(const package::Reference &ref) noexcept;
    utils::error::Result<void> removeCache(const package::Reference &ref) noexcept;
    utils::error::Result<void> executePostInstallHooks(const package::Reference &ref) noexcept;
    utils::error::Result<void> executePostUninstallHooks(const package::Reference &ref) noexcept;
    linglong::repo::OSTreeRepo &repo; // NOLINT
    PackageTaskQueue tasks;

    JobQueue m_search_queue = {};
    JobQueue m_prune_queue = {};
    JobQueue m_generator_queue = {};

    int lockFd{ -1 };
    linglong::runtime::ContainerBuilder &containerBuilder;
};

} // namespace linglong::service
