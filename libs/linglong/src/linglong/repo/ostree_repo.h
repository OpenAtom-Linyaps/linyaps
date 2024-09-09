/*
 * SPDX-FileCopyrightText: 2022-2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/task.h"
#include "linglong/repo/client_factory.h"
#include "linglong/repo/repo_cache.h"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <QList>
#include <QPointer>
#include <QProcess>
#include <QScopedPointer>
#include <QThread>

namespace linglong::repo {

struct clearReferenceOption
{
    bool forceRemote = false;
    bool fallbackToRemote = true;
};

class OSTreeRepo : public QObject
{
    Q_OBJECT
public:
    OSTreeRepo(const OSTreeRepo &) = delete;
    OSTreeRepo(OSTreeRepo &&) = delete;
    OSTreeRepo &operator=(const OSTreeRepo &) = delete;
    OSTreeRepo &operator=(OSTreeRepo &&) = delete;
    OSTreeRepo(const QDir &path,
               const api::types::v1::RepoConfig &cfg,
               ClientFactory &clientFactory) noexcept;

    ~OSTreeRepo() override;

    [[nodiscard]] const api::types::v1::RepoConfig &getConfig() const noexcept;
    utils::error::Result<void> setConfig(const api::types::v1::RepoConfig &cfg) noexcept;

    utils::error::Result<package::LayerDir>
    importLayerDir(const package::LayerDir &dir,
                   const std::optional<std::string> &subRef = std::nullopt) noexcept;

    [[nodiscard]] utils::error::Result<package::LayerDir>
    getLayerDir(const package::Reference &ref,
                const std::string &module = "binary",
                const std::optional<std::string> &subRef = std::nullopt) const noexcept;
    [[nodiscard]] utils::error::Result<void>
    push(const package::Reference &reference, const std::string &module = "binary") const noexcept;

    [[nodiscard]] utils::error::Result<void>
    pushToRemote(const std::string &remoteRepo,
                 const std::string &url,
                 const package::Reference &reference,
                 const std::string &module = "binary") const noexcept;

    void pull(service::InstallTask &taskContext,
              const package::Reference &reference,
              const std::string &module = "binary") noexcept;

    [[nodiscard]] utils::error::Result<package::Reference> clearReference(
      const package::FuzzyReference &fuzz, const clearReferenceOption &opts) const noexcept;

    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>> listLocal() const noexcept;
    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
    listRemote(const package::FuzzyReference &fuzzyRef) const noexcept;

    utils::error::Result<void>
    remove(const package::Reference &ref,
           const std::string &module = "binary",
           const std::optional<std::string> &subRef = std::nullopt) noexcept;

    utils::error::Result<void> prune();

    void removeDanglingXDGIntergation() noexcept;
    // exportReference should be called when LayerDir of ref is existed in local repo
    void exportReference(const package::Reference &ref) noexcept;
    // unexportReference should be called when LayerDir of ref is existed in local repo
    void unexportReference(const package::Reference &ref) noexcept;
    void updateSharedInfo() noexcept;

private:
    api::types::v1::RepoConfig cfg;

    struct OstreeRepoDeleter
    {
        void operator()(OstreeRepo *repo)
        {
            qDebug() << "delete OstreeRepo" << repo;
            g_clear_object(&repo);
        }
    };

    std::unique_ptr<OstreeRepo, OstreeRepoDeleter> ostreeRepo = nullptr;
    QDir repoDir;
    std::unique_ptr<linglong::repo::RepoCache> cache{ nullptr };
    ClientFactory &m_clientFactory;

    utils::error::Result<void> updateConfig(const api::types::v1::RepoConfig &newCfg) noexcept;
    QDir ostreeRepoDir() const noexcept;
    QDir createLayerQDir(const std::string &commit) const noexcept;
    utils::error::Result<void> handleRepositoryUpdate(
      QDir layerDir, const api::types::v1::RepositoryCacheLayersItem &layer) noexcept;
    utils::error::Result<void>
    removeOstreeRef(const api::types::v1::RepositoryCacheLayersItem &layer) noexcept;
    [[nodiscard]] utils::error::Result<package::LayerDir>
    getLayerDir(const api::types::v1::RepositoryCacheLayersItem &layer) const noexcept;
    utils::error::Result<void> migrate() noexcept;

    [[nodiscard]] utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
    getLayerItem(const package::Reference &ref,
                 const std::string &module,
                 const std::optional<std::string> &subRef = std::nullopt) const noexcept;
};

} // namespace linglong::repo
