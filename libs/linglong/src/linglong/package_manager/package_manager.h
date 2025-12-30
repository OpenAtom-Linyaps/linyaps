/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/api/types/v1/InteractionMessageType.hpp"
#include "linglong/api/types/v1/PackageManager1RequestInteractionAdditionalMessage.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/log/log.h"
#include "package_task.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>

#include <optional>

namespace linglong::service {

class Action;

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
    auto Update(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Search(const QVariantMap &parameters) noexcept -> QVariantMap;
    auto Prune() noexcept -> QVariantMap;
    void ReplyInteraction(QDBusObjectPath object_path, const QVariantMap &replies);

    // Nothing to do here, Permissions() will be rejected in org.deepin.linglong.PackageManager.conf
    void Permissions() { }

    bool waitConfirm(PackageTask &taskRef,
                     api::types::v1::InteractionMessageType msgType,
                     const api::types::v1::PackageManager1RequestInteractionAdditionalMessage
                       &additionalMessage) noexcept;

    virtual utils::error::Result<void> applyApp(const package::Reference &reference) noexcept;
    virtual utils::error::Result<void> unapplyApp(const package::Reference &reference) noexcept;
    virtual utils::error::Result<void> switchAppVersion(const package::Reference &oldRef,
                                                        const package::Reference &newRef,
                                                        bool removeOldRef = false) noexcept;
    virtual utils::error::Result<void> tryGenerateCache(const package::Reference &ref) noexcept;
    utils::error::Result<void> executePostInstallHooks(const package::Reference &ref) noexcept;
    utils::error::Result<void> executePostUninstallHooks(const package::Reference &ref) noexcept;

    virtual utils::error::Result<void> installAppDepends(Task &task,
                                                         const api::types::v1::PackageInfoV2 &app);
    virtual utils::error::Result<void>
    installDependsRef(Task &task,
                      const std::string &refStr,
                      std::optional<std::string> channel = std::nullopt,
                      std::optional<std::string> version = std::nullopt);
    virtual utils::error::Result<void> installRef(Task &task,
                                                  const package::ReferenceWithRepo &ref,
                                                  std::vector<std::string> modules) noexcept;
    utils::error::Result<void> Uninstall(PackageTask &taskContext,
                                         const package::Reference &ref,
                                         const std::string &module) noexcept;
    virtual utils::error::Result<bool> tryUninstallRef(const package::Reference &ref) noexcept;
    utils::error::Result<void>
    uninstallRef(const package::Reference &ref,
                 std::optional<std::vector<std::string>> modules = std::nullopt) noexcept;

Q_SIGNALS:
    void TaskAdded(QDBusObjectPath object_path);
    void TaskRemoved(QDBusObjectPath object_path);
    void RequestInteraction(QDBusObjectPath object_path,
                            int messageID,
                            QVariantMap additionalMessage);
    void SearchFinished(QString jobID, QVariantMap result);
    void PruneFinished(QString jobID, QVariantMap result);
    void ReplyReceived(const QVariantMap &replies);

private:
    QVariantMap installFromLayer(const QDBusUnixFileDescriptor &fd,
                                 const api::types::v1::CommonOptions &options) noexcept;

    QVariantMap installFromUAB(const QDBusUnixFileDescriptor &fd,
                               const api::types::v1::CommonOptions &options) noexcept;

    [[nodiscard]] utils::error::Result<void> lockRepo() noexcept;
    [[nodiscard]] utils::error::Result<void> unlockRepo() noexcept;
    [[nodiscard]] static utils::error::Result<
      std::vector<api::types::v1::ContainerProcessStateInfo>>
    getAllRunningContainers() noexcept;
    utils::error::Result<bool> isRefBusy(const package::Reference &ref) noexcept;
    void deferredUninstall() noexcept;
    utils::error::Result<void>
    Prune(std::vector<api::types::v1::PackageInfoV2> &removedInfo) noexcept;
    utils::error::Result<void> removeCache(const package::Reference &ref) noexcept;

    QVariantMap runActionOnTaskQueue(std::shared_ptr<Action> action);

    linglong::repo::OSTreeRepo &repo; // NOLINT
    PackageTaskQueue tasks;
    PackageTaskQueue m_search_queue;

    int lockFd{ -1 };
    linglong::runtime::ContainerBuilder &containerBuilder;
};

} // namespace linglong::service
