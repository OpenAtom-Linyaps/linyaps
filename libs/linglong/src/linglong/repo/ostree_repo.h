/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/task.h"
#include "linglong/repo/client_factory.h"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <QHttpPart>
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

    api::types::v1::RepoConfig getConfig() const noexcept;
    utils::error::Result<void> setConfig(const api::types::v1::RepoConfig &cfg) noexcept;

    utils::error::Result<void> importLayerDir(const package::LayerDir &dir) noexcept;

    utils::error::Result<package::LayerDir> getLayerDir(const package::Reference &ref,
                                                        bool develop = false) const noexcept;

    utils::error::Result<void> push(const package::Reference &reference,
                                    bool develop = false) const noexcept;

    void pull(service::InstallTask &taskContext,
              const package::Reference &reference,
              bool develop = false) noexcept;

    utils::error::Result<package::Reference> clearReference(
      const package::FuzzyReference &fuzz, const clearReferenceOption &opts) const noexcept;

    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>> listLocal() const noexcept;
    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
    listRemote(const package::FuzzyReference &fuzzyRef) const noexcept;

    utils::error::Result<void> remove(const package::Reference &ref, bool develop = false) noexcept;

    void removeDanglingXDGIntergation() noexcept;
    void exportReference(const package::Reference &ref) noexcept;
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
    QDir ostreeRepoDir() const noexcept;
    QDir getLayerQDir(const package::Reference &ref, bool develop = false) const noexcept;
    QDir getLayerQDirV2(const package::Reference &ref, bool develop = false) const noexcept;

    ClientFactory &m_clientFactory;
};

} // namespace linglong::repo

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
