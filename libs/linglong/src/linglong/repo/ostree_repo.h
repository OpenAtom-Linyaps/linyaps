/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
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

    api::types::v1::RepoConfig getConfig() const noexcept;
    utils::error::Result<void> setConfig(const api::types::v1::RepoConfig &cfg) noexcept;

    utils::error::Result<package::LayerDir> importLayerDir(const package::LayerDir &dir,
                                                           const QString &subRef = "") noexcept;

    utils::error::Result<package::LayerDir> getLayerDir(const package::Reference &ref,
                                                        const QString &module = "binary",
                                                        const QString &subRef = "") const noexcept;

    Q_DECL_DEPRECATED_X(R"(Use the "module" version)")

    utils::error::Result<package::LayerDir> getLayerDir(const package::Reference &ref,
                                                        bool develop,
                                                        const QString &subRef = "") const noexcept
    {
        if (develop) {
            return getLayerDir(ref, QString("develop"), subRef);
        }
        return getLayerDir(ref, QString("binary"), subRef);
    }

    utils::error::Result<void> push(const package::Reference &reference,
                                    const QString &module = "binary") const noexcept;

    Q_DECL_DEPRECATED_X(R"(Use the "module" version)")

    utils::error::Result<void> push(const package::Reference &reference,
                                    bool develop) const noexcept
    {
        if (develop) {
            return push(reference, QString("develop"));
        }
        return push(reference, QString("binary"));
    }

    void pull(service::InstallTask &taskContext,
              const package::Reference &reference,
              const QString &module = "binary") noexcept;

    Q_DECL_DEPRECATED_X(R"(Use the "module" version)")

    void pull(service::InstallTask &taskContext,
              const package::Reference &reference,
              bool develop) noexcept
    {
        if (develop) {
            pull(taskContext, reference, QString("develop"));
            return;
        }
        pull(taskContext, reference, QString("binary"));
    }

    utils::error::Result<package::Reference> clearReference(
      const package::FuzzyReference &fuzz, const clearReferenceOption &opts) const noexcept;

    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>> listLocal() const noexcept;
    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
    listRemote(const package::FuzzyReference &fuzzyRef) const noexcept;

    utils::error::Result<void> remove(const package::Reference &ref,
                                      const QString &module = "binary",
                                      const QString &subRef = "") noexcept;

    Q_DECL_DEPRECATED_X(R"(Use the "module" version)")

    utils::error::Result<void> remove(const package::Reference &ref,
                                      bool develop,
                                      const QString &subRef = "") noexcept
    {
        if (develop) {
            return remove(ref, QString("develop"), subRef);
        }
        return remove(ref, QString("binary"), subRef);
    }

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
    QDir ostreeRepoDir() const noexcept;
    QDir createLayerQDir(const package::Reference &ref,
                         const QString &module = "binary",
                         const QString &subRef = "") const noexcept;

    ClientFactory &m_clientFactory;
};

} // namespace linglong::repo
