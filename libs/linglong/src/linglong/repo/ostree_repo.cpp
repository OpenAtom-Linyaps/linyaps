/*
 * SPDX-FileCopyrightText: 2022-2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "api/ClientAPI.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/api/types/v1/RepositoryCacheMergedItem.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/config.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gkeyfile_wrapper.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/transaction.h"

#include <gio/gio.h>
#include <glib.h>
#include <nlohmann/json_fwd.hpp>
#include <ostree-repo.h>

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::repo {

namespace {

struct ostreeUserData
{
    bool scanning{ false };
    bool caught_error{ false };
    bool last_was_metadata{ false };
    guint outstanding_fetches{ 0 };
    guint outstanding_writes{ 0 };
    guint fetched{ 0 };
    guint requested{ 0 };
    guint scanned_metadata{ 0 };
    guint bytes_transferred{ 0 };
    guint fetched_delta_parts{ 0 };
    guint total_delta_parts{ 0 };
    guint fetched_delta_fallbacks{ 0 };
    guint total_delta_fallbacks{ 0 };
    guint total_delta_superblocks{ 0 };
    guint outstanding_metadata_fetches{ 0 };
    guint metadata_fetched{ 0 };
    guint64 fetched_delta_part_size{ 0 };
    guint64 total_delta_part_size{ 0 };
    guint64 total_delta_part_usize{ 0 };
    char *ostree_status{ nullptr };
    service::PackageTask *taskContext{ nullptr };
    std::string status{ "Beginning to pull data" };
    long double progress{ 0 };
    long double last_total{ 0 };

    ~ostreeUserData() { g_clear_pointer(&ostree_status, g_free); }
};

void progress_changed(OstreeAsyncProgress *progress, gpointer user_data)
{
    auto *data = static_cast<ostreeUserData *>(user_data);
    g_clear_pointer(&data->ostree_status, g_free);

    if (data->caught_error) {
        return;
    }

    ostree_async_progress_get(progress,
                              "outstanding-fetches",
                              "u",
                              &data->outstanding_fetches,
                              "outstanding-writes",
                              "u",
                              &data->outstanding_writes,
                              "fetched",
                              "u",
                              &data->fetched,
                              "requested",
                              "u",
                              &data->requested,
                              "scanning",
                              "u",
                              &data->scanning,
                              "caught-error",
                              "b",
                              &data->caught_error,
                              "scanned-metadata",
                              "u",
                              &data->scanned_metadata,
                              "bytes-transferred",
                              "t",
                              &data->bytes_transferred,
                              "fetched-delta-parts",
                              "u",
                              &data->fetched_delta_parts,
                              "total-delta-parts",
                              "u",
                              &data->total_delta_parts,
                              "fetched-delta-fallbacks",
                              "u",
                              &data->fetched_delta_fallbacks,
                              "total-delta-fallbacks",
                              "u",
                              &data->total_delta_fallbacks,
                              "fetched-delta-part-size",
                              "t",
                              &data->fetched_delta_part_size,
                              "total-delta-part-size",
                              "t",
                              &data->total_delta_part_size,
                              "total-delta-part-usize",
                              "t",
                              &data->total_delta_part_usize,
                              "total-delta-superblocks",
                              "u",
                              &data->total_delta_superblocks,
                              "outstanding-metadata-fetches",
                              "u",
                              &data->outstanding_metadata_fetches,
                              "metadata-fetched",
                              "u",
                              &data->metadata_fetched,
                              "status",
                              "s",
                              &data->ostree_status,
                              nullptr);

    long double total{ 0 };
    long double new_progress{ 0 };
    guint64 total_transferred{ 0 };
    bool last_was_metadata{ data->last_was_metadata };

    auto updateProgress = utils::finally::finally([&new_progress, data, &total] {
        LINGLONG_TRACE("update progress status")

        if (data->caught_error) {
            data->taskContext->reportError(
              LINGLONG_ERRV("Caught error during pulling data, waiting for outstanding task"));
            return;
        }

        new_progress = std::max(new_progress, data->progress);
        data->last_total = total;
        if (new_progress > 100) {
            qDebug() << "progress overflow, limiting to 100";
            new_progress = 100;
        }

        data->progress = new_progress;
        data->taskContext->updateTask(static_cast<double>(data->progress),
                                      100,
                                      QString::fromStdString(data->status));
    });

    if (data->requested == 0) {
        return;
    }

    if (*(data->ostree_status) != 0) {
        new_progress = 100;
        return;
    }

    total_transferred = data->bytes_transferred;
    data->last_was_metadata = false;
    if (data->total_delta_parts == 0
        && (data->outstanding_metadata_fetches > 0 || last_was_metadata)
        && data->metadata_fetched < 20) {
        if (data->outstanding_metadata_fetches > 0) {
            data->last_was_metadata = true;
        }

        data->status = "Downloading metadata";
        new_progress = 0;
        if (data->progress > 0) {
            new_progress = (data->fetched * 5.0) / data->requested;
        }
    } else {
        if (data->total_delta_parts > 0) {
            total = data->total_delta_part_size - data->fetched_delta_part_size;
            data->status = "Downloading delta part";
        } else {
            auto average_object_size{ 1.0 };
            if (data->fetched > 0) {
                average_object_size = static_cast<double>(data->bytes_transferred) / data->fetched;
            }

            total = average_object_size * data->requested;
            data->status = "Downloading files";
        }
    }
    Q_EMIT data->taskContext->PartChanged(data->fetched, data->requested);
    new_progress = total > 0 ? 5 + ((total_transferred / total) * 92) : 97;
    new_progress += (data->outstanding_writes > 0 ? (3.0 / data->outstanding_writes) : 3.0);
}

std::string ostreeRefFromLayerItem(const api::types::v1::RepositoryCacheLayersItem &layer)
{
    std::string refspec = layer.info.channel + "/" + layer.info.id + "/" + layer.info.version + "/"
      + layer.info.arch.front() + "/" + layer.info.packageInfoV2Module;

    return refspec;
}

std::string ostreeRefSpecFromLayerItem(const api::types::v1::RepositoryCacheLayersItem &layer)
{
    std::string refspec = layer.repo + ":" + ostreeRefFromLayerItem(layer);
    return refspec;
}

std::string ostreeSpecFromReference(const package::Reference &ref,
                                    const std::optional<std::string> &repo = std::nullopt,
                                    std::string module = "binary") noexcept
{
    if (module == "binary") {
        module = "runtime";
    }

    auto spec = ref.channel.toStdString() + "/" + ref.id.toStdString() + "/"
      + ref.version.toString().toStdString() + "/" + ref.arch.toString().toStdString() + "/"
      + module;

    if (repo) {
        spec = repo.value() + ":" + spec;
    }
    return spec;
}

std::string
ostreeSpecFromReferenceV2(const package::Reference &ref,
                          const std::optional<std::string> &repo = std::nullopt,
                          std::string module = "binary",
                          const std::optional<std::string> &subRef = std::nullopt) noexcept
{
    if (module == "runtime") {
        module = "binary";
    }
    auto ret = ref.channel.toStdString() + "/" + ref.id.toStdString() + "/"
      + ref.version.toString().toStdString() + "/" + ref.arch.toString().toStdString() + "/"
      + module;

    if (repo) {
        ret = repo.value() + ":" + ret;
    }
    if (!subRef) {
        return ret;
    }

    return ret + "_" + subRef.value();
}

utils::error::Result<QString> commitDirToRepo(std::vector<GFile *> dirs,
                                              OstreeRepo *repo,
                                              const char *refspec) noexcept
{
    Q_ASSERT(dirs.size() >= 1);
    Q_ASSERT(repo != nullptr);

    LINGLONG_TRACE("commit to ostree linglong repo");

    g_autoptr(GError) gErr = nullptr;
    utils::Transaction transaction;
    if (ostree_repo_prepare_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_prepare_transaction", gErr);
    }

    transaction.addRollBack([repo]() noexcept {
        g_autoptr(GError) gErr = nullptr;
        if (ostree_repo_abort_transaction(repo, nullptr, &gErr) == FALSE) {
            qCritical() << "ostree_repo_abort_transaction:" << gErr->message << gErr->code;
        }
    });

    g_autoptr(OstreeMutableTree) mtree = ostree_mutable_tree_new();
    g_autoptr(OstreeRepoCommitModifier) modifier = nullptr;
    modifier =
      ostree_repo_commit_modifier_new(OSTREE_REPO_COMMIT_MODIFIER_FLAGS_CANONICAL_PERMISSIONS,
                                      nullptr,
                                      nullptr,
                                      nullptr);
    Q_ASSERT(modifier != nullptr);
    if (modifier == nullptr) {
        return LINGLONG_ERR("ostree_repo_commit_modifier_new return a nullptr");
    }

    for (auto *dir : dirs) {
        if (ostree_repo_write_directory_to_mtree(repo, dir, mtree, modifier, nullptr, &gErr)
            == FALSE) {
            return LINGLONG_ERR("ostree_repo_write_directory_to_mtree", gErr);
        }
    }

    g_autoptr(GFile) file = nullptr;
    if (ostree_repo_write_mtree(repo, mtree, &file, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_write_mtree", gErr);
    }

    g_autofree char *commit = nullptr;
    if (ostree_repo_write_commit(repo,
                                 nullptr,
                                 nullptr,
                                 nullptr,
                                 nullptr,
                                 OSTREE_REPO_FILE(file),
                                 &commit,
                                 NULL,
                                 &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_write_commit", gErr);
    }

    ostree_repo_transaction_set_ref(repo, "local", refspec, commit);

    transaction.commit();

    if (ostree_repo_commit_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_commit_transaction", gErr);
    }

    return commit;
}

utils::error::Result<void> updateOstreeRepoConfig(OstreeRepo *repo,
                                                  const QString &remoteName,
                                                  const QString &url,
                                                  const QString &parent = "") noexcept
{
    LINGLONG_TRACE("update configuration");

    g_autoptr(GVariant) options = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "gpg-verify", g_variant_new_boolean(FALSE));
    // NOTE:
    // libcurl 8.2.1 has a http2 bug https://github.com/curl/curl/issues/11859
    // We disable http2 for now.
    g_variant_builder_add(&builder, "{sv}", "http2", g_variant_new_boolean(FALSE));
    options = g_variant_ref_sink(g_variant_builder_end(&builder));

    g_autoptr(GError) gErr = nullptr;
    if (ostree_repo_remote_change(repo,
                                  nullptr,
                                  OSTREE_REPO_REMOTE_CHANGE_DELETE_IF_EXISTS,
                                  remoteName.toUtf8(),
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_remote_change", gErr);
    }

    if (ostree_repo_remote_change(repo,
                                  nullptr,
                                  OSTREE_REPO_REMOTE_CHANGE_ADD,
                                  remoteName.toUtf8(),
                                  (url + "/repos/" + remoteName).toUtf8(),
                                  options,
                                  nullptr,
                                  &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_remote_change", gErr);
    }

    GKeyFile *configKeyFile = ostree_repo_get_config(repo);
    Q_ASSERT(configKeyFile != nullptr);
    QDir a;

    g_key_file_set_string(configKeyFile, "core", "min-free-space-size", "600MB");
    if (!parent.isEmpty()) {
        QDir parentDir = parent;
        Q_ASSERT(parentDir.exists());
        g_key_file_set_string(configKeyFile, "core", "parent", parentDir.absolutePath().toUtf8());
    }

    if (ostree_repo_write_config(repo, configKeyFile, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_write_config", gErr);
    }

    return LINGLONG_OK;
}

utils::error::Result<OstreeRepo *> createOstreeRepo(const QDir &location,
                                                    const QString &remoteName,
                                                    const QString &url,
                                                    const QString &parent = "") noexcept
{
    LINGLONG_TRACE("create linglong repository at " + location.absolutePath());

    if (!QFileInfo(location.absolutePath()).isWritable()) {
        return LINGLONG_ERR("permission denied");
    }

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) repoPath = nullptr;
    g_autoptr(OstreeRepo) ostreeRepo = nullptr;

    repoPath = g_file_new_for_path(location.absolutePath().toUtf8());
    ostreeRepo = ostree_repo_new(repoPath);
    Q_ASSERT(ostreeRepo != nullptr);
    if (ostree_repo_create(ostreeRepo, OSTREE_REPO_MODE_BARE_USER_ONLY, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_create", gErr);
    }

    auto result = updateOstreeRepoConfig(ostreeRepo, remoteName, url, parent);

    if (!result) {
        return LINGLONG_ERR(result);
    }

    return static_cast<OstreeRepo *>(g_steal_pointer(&ostreeRepo));
}

utils::error::Result<package::Reference> clearReferenceLocal(const linglong::repo::RepoCache &cache,
                                                             package::FuzzyReference fuzzy) noexcept
{
    LINGLONG_TRACE("clear fuzzy reference locally");

    // the arch of all local packages is host arch
    if (fuzzy.arch) {
        auto curArch = linglong::package::Architecture::currentCPUArchitecture();
        if (!curArch) {
            return LINGLONG_ERR(curArch);
        }
        if (curArch->toString() != fuzzy.arch->toString()) {
            return LINGLONG_ERR("arch mismatch with host arch");
        }
    }

    // NOTE: ignore channel, two packages with the same version but different channels are not
    // allowed to be installed
    repoCacheQuery query;
    query.id = fuzzy.id.toStdString();
    const auto availablePackage = cache.queryLayerItem(query);
    if (availablePackage.empty()) {
        return LINGLONG_ERR("package not found:" % fuzzy.toString());
    }

    utils::error::Result<linglong::api::types::v1::RepositoryCacheLayersItem> foundRef =
      LINGLONG_ERR("compatible layer not found");
    for (const auto &ref : availablePackage) {
        // we should ignore deleted layers
        if (ref.deleted && ref.deleted.value()) {
            continue;
        }

        auto ver = QString::fromStdString(ref.info.version);
        auto pkgVer = linglong::package::Version::parse(ver);
        if (!pkgVer) {
            qFatal("internal error: broken data of repo cache: %s", ver.toStdString().c_str());
        }

        qDebug() << "available layer found:" << fuzzy.toString() << ver;
        if (fuzzy.version) {
            if (!fuzzy.version->tweak) {
                pkgVer->tweak = std::nullopt;
            }

            if (*pkgVer == fuzzy.version.value()) {
                foundRef = ref;
                break;
            }

            continue;
        }

        foundRef = ref;
        break;
    }

    if (!foundRef) {
        return LINGLONG_ERR(foundRef);
    }

    auto ver = linglong::package::Version::parse(QString::fromStdString(foundRef->info.version));
    auto arch = linglong::package::Architecture::parse(foundRef->info.arch[0]);
    return package::Reference::create(QString::fromStdString(foundRef->info.channel),
                                      QString::fromStdString(foundRef->info.id),
                                      *ver,
                                      *arch);
};

} // namespace

utils::error::Result<void>
OSTreeRepo::removeOstreeRef(const api::types::v1::RepositoryCacheLayersItem &layer) noexcept
{
    LINGLONG_TRACE("remove ostree refspec from repository");

    std::string refspec = ostreeRefSpecFromLayerItem(layer);
    std::string ref = ostreeRefFromLayerItem(layer);

    g_autoptr(GError) gErr = nullptr;
    g_autofree char *rev{ nullptr };

    if (ostree_repo_resolve_rev_ext(this->ostreeRepo.get(),
                                    refspec.c_str(),
                                    FALSE,
                                    OstreeRepoResolveRevExtFlags::OSTREE_REPO_RESOLVE_REV_EXT_NONE,
                                    &rev,
                                    &gErr)
        == FALSE) {
        return LINGLONG_ERR(QString{ "couldn't resolve ref %1 on local machine" }.arg(
                              QString::fromStdString(refspec)),
                            gErr);
    }

    if (ostree_repo_set_ref_immediate(this->ostreeRepo.get(),
                                      layer.repo.c_str(),
                                      ref.c_str(),
                                      nullptr,
                                      nullptr,
                                      &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_set_ref_immediate", gErr);
    }

    auto ret = this->cache->deleteLayerItem(layer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::handleRepositoryUpdate(
  QDir layerDir, const api::types::v1::RepositoryCacheLayersItem &layer) noexcept
{
    std::string refspec = ostreeRefSpecFromLayerItem(layer);
    LINGLONG_TRACE(QString("checkout %1 from ostree repository to layers dir")
                     .arg(QString::fromStdString(refspec)));

    int root = open("/", O_DIRECTORY);
    auto _ = utils::finally::finally([root]() {
        close(root);
    });

    auto path = layerDir.absolutePath();
    path = path.right(path.length() - 1);

    if (!layerDir.mkpath(".")) {
        Q_ASSERT(false);
        return LINGLONG_ERR(QString{ "couldn't create directory %1" }.arg(layerDir.absolutePath()));
    }

    if (!layerDir.removeRecursively()) {
        Q_ASSERT(false);
        return LINGLONG_ERR(QString{ "couldn't remove directory %1" }.arg(layerDir.absolutePath()));
    }

    g_autoptr(GError) gErr = nullptr;
    g_autofree char *commit{ nullptr };

    if (ostree_repo_resolve_rev_ext(this->ostreeRepo.get(),
                                    refspec.c_str(),
                                    FALSE,
                                    OstreeRepoResolveRevExtFlags::OSTREE_REPO_RESOLVE_REV_EXT_NONE,
                                    &commit,
                                    &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_resolve_rev", gErr);
    }

    if (ostree_repo_checkout_at(this->ostreeRepo.get(),
                                nullptr,
                                root,
                                path.toUtf8().constData(),
                                commit,
                                nullptr,
                                &gErr)
        == FALSE) {
        return LINGLONG_ERR(QString("ostree_repo_checkout_at %1").arg(path), gErr);
    }

    auto ret = this->cache->addLayerItem(layer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    return LINGLONG_OK;
}

utils::error::Result<QDir> OSTreeRepo::ensureEmptyLayerDir(const std::string &commit) const noexcept
{
    LINGLONG_TRACE(
      QString{ "ensure empty layer dir exist %1" }.arg(QString::fromStdString(commit)));

    std::filesystem::path dir =
      this->repoDir.absoluteFilePath(QString::fromStdString("layers/" + commit)).toStdString();

    std::error_code ec;
    std::optional<std::filesystem::path> target;
    if (std::filesystem::is_symlink(dir, ec)) {
        target = std::filesystem::read_symlink(dir, ec);
        if (ec) {
            return LINGLONG_ERR(
              QString{ "failed to resolve symlink %1: %2" }.arg(dir.c_str(), ec.message().c_str()));
        }

        if (!std::filesystem::remove(dir, ec)) {
            return LINGLONG_ERR(
              QString{ "failed to remove symlink %1: %2" }.arg(dir.c_str(), ec.message().c_str()));
        }
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return LINGLONG_ERR(
          QString{ "check %1 is symlink failed: %2" }.arg(dir.c_str(), ec.message().c_str()));
    }

    if (target && std::filesystem::exists(*target, ec)) {
        if (std::filesystem::remove_all(*target, ec) == static_cast<std::uintmax_t>(-1)) {
            return LINGLONG_ERR(
              QString{ "failed to remove layer dir %1: %2" }.arg(target->c_str(),
                                                                 ec.message().c_str()));
        }
    }

    if (!std::filesystem::create_directories(dir, ec)) {
        return LINGLONG_ERR(
          QString{ "failed to create layer dir %1: %2" }.arg(dir.c_str(), ec.message().c_str()));
    }

    return QDir{ dir.c_str() };
}

QDir OSTreeRepo::ostreeRepoDir() const noexcept
{
    Q_ASSERT(!this->repoDir.path().isEmpty());
    auto repoDir = QDir(this->repoDir.absoluteFilePath("repo"));
    if (!repoDir.mkpath(".")) {
        Q_ASSERT(false);
    }
    return repoDir;
}

OSTreeRepo::OSTreeRepo(const QDir &path,
                       const api::types::v1::RepoConfigV2 &cfg,
                       ClientFactory &clientFactory) noexcept
    : cfg(cfg)
    , m_clientFactory(clientFactory)
{
    if (!path.exists()) {
        qFatal("repo doesn't exists");
    }

    if (!QFileInfo(path.absolutePath()).isReadable()) {
        auto msg = QString("read linglong repository(%1): permission denied ")
                     .arg(path.path())
                     .toStdString();
        qFatal("%s", msg.c_str());
    }

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) repoPath = nullptr;
    g_autoptr(OstreeRepo) ostreeRepo = nullptr;

    this->repoDir = path;
    {
        LINGLONG_TRACE("use linglong repo at " + path.absolutePath());

        repoPath = g_file_new_for_path(this->ostreeRepoDir().absolutePath().toUtf8());
        ostreeRepo = ostree_repo_new(repoPath);
        Q_ASSERT(ostreeRepo != nullptr);
        if (ostree_repo_open(ostreeRepo, nullptr, &gErr) == TRUE) {
            this->ostreeRepo.reset(static_cast<OstreeRepo *>(g_steal_pointer(&ostreeRepo)));

            auto ret = linglong::repo::RepoCache::create(
              this->repoDir.absoluteFilePath("states.json").toStdString(),
              this->cfg,
              *(this->ostreeRepo));
            if (!ret) {
                qCritical() << LINGLONG_ERRV(ret);
                qFatal("abort");
            }
            this->cache = std::move(ret).value();

            return;
        }

        qDebug() << LINGLONG_ERRV("ostree_repo_open", gErr);

        g_clear_error(&gErr);
        g_clear_object(&ostreeRepo);
    }

    LINGLONG_TRACE("init ostree-based linglong repository");

    const auto defaultRepo = getDefaultRepo(this->cfg);
    auto result = createOstreeRepo(this->ostreeRepoDir().absolutePath(),
                                   QString::fromStdString(defaultRepo.name),
                                   QString::fromStdString(defaultRepo.url));
    if (!result) {
        qCritical() << LINGLONG_ERRV(result);
        qFatal("abort");
    }

    this->ostreeRepo.reset(*result);

    auto ret =
      linglong::repo::RepoCache::create(this->repoDir.absoluteFilePath("states.json").toStdString(),
                                        this->cfg,
                                        *(this->ostreeRepo));
    if (!ret) {
        qCritical() << LINGLONG_ERRV(ret);
        qFatal("abort");
    }

    this->cache = std::move(ret).value();
}

const api::types::v1::RepoConfigV2 &OSTreeRepo::getConfig() const noexcept
{
    return cfg;
}

utils::error::Result<void>
OSTreeRepo::updateConfig(const api::types::v1::RepoConfigV2 &newCfg) noexcept
{
    LINGLONG_TRACE("update underlying config")

    auto result = saveConfig(newCfg, this->repoDir.absoluteFilePath("config.yaml"));
    if (!result) {
        return LINGLONG_ERR(result);
    }

    utils::Transaction transaction;
    const auto newRepo = getDefaultRepo(newCfg);
    result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                    QString::fromStdString(newRepo.name),
                                    QString::fromStdString(newRepo.url));
    transaction.addRollBack([this]() noexcept {
        const auto defaultRepo = getDefaultRepo(this->cfg);
        auto result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                             QString::fromStdString(defaultRepo.name),
                                             QString::fromStdString(defaultRepo.url));
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();

    this->m_clientFactory.setServer(QString::fromStdString(newRepo.url));
    this->cfg = newCfg;

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::setConfig(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    LINGLONG_TRACE("set config");

    utils::Transaction transaction;

    auto result = saveConfig(cfg, this->repoDir.absoluteFilePath("config.yaml"));
    if (!result) {
        return LINGLONG_ERR(result);
    }
    transaction.addRollBack([this]() noexcept {
        auto result = saveConfig(this->cfg, this->repoDir.absoluteFilePath("config.yaml"));
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    const auto newRepo = getDefaultRepo(cfg);
    result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                    QString::fromStdString(newRepo.name),
                                    QString::fromStdString(newRepo.url));
    if (!result) {
        return LINGLONG_ERR(result);
    }
    transaction.addRollBack([this]() noexcept {
        const auto defaultRepo = getDefaultRepo(this->cfg);
        auto result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                             QString::fromStdString(defaultRepo.name),
                                             QString::fromStdString(defaultRepo.url));
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    if (auto ret = this->cache->rebuildCache(cfg, *(this->ostreeRepo)); !ret) {
        return LINGLONG_ERR(ret);
    }

    this->m_clientFactory.setServer(newRepo.url);
    this->cfg = cfg;

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<package::LayerDir>
OSTreeRepo::importLayerDir(const package::LayerDir &dir,
                           std::vector<std::filesystem::path> overlays,
                           const std::optional<std::string> &subRef) noexcept
{
    LINGLONG_TRACE("import layer dir");

    if (!dir.exists()) {
        return LINGLONG_ERR(QString("layer directory %1 not exists").arg(dir.absolutePath()));
    }

    utils::Transaction transaction;

    auto info = dir.info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    auto reference = package::Reference::fromPackageInfo(*info);
    if (!reference) {
        return LINGLONG_ERR(reference);
    }

    if (this->getLayerDir(*reference, info->packageInfoV2Module, subRef)) {
        return LINGLONG_ERR(reference->toString() + " exists.", 0);
    }

    overlays.insert(overlays.begin(), dir.absolutePath().toStdString());

    std::vector<GFile *> dirs;
    auto cleanRes = utils::finally::finally([&dirs] {
        std::for_each(dirs.begin(), dirs.end(), [](GFile *file) {
            g_object_unref(file);
        });
    });

    for (const auto &overlay : overlays) {
        auto *gFile = g_file_new_for_path(overlay.c_str());
        if (gFile == nullptr) {
            qFatal("g_file_new_for_path");
        }
        dirs.push_back(gFile);
    }

    // NOTE: we save repo info in cache, if import a local layer dir, set repo to 'local'
    auto refspec =
      ostreeSpecFromReferenceV2(*reference, std::nullopt, info->packageInfoV2Module, subRef);
    auto commitID = commitDirToRepo(dirs, this->ostreeRepo.get(), refspec.c_str());
    if (!commitID) {
        return LINGLONG_ERR(commitID);
    }

    api::types::v1::RepositoryCacheLayersItem item;

    item.commit = (*commitID).toStdString();
    item.info = *info;
    item.repo = "local";

    auto layerDir = this->ensureEmptyLayerDir((*commitID).toStdString());
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto result = this->handleRepositoryUpdate(*layerDir, item);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.addRollBack([this, &layerDir, &item]() noexcept {
        if (!layerDir->removeRecursively()) {
            qCritical() << "remove layer dir failed: " << layerDir->absolutePath();
            Q_ASSERT(false);
        }
        auto result = this->removeOstreeRef(item);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    transaction.commit();
    return package::LayerDir{ layerDir->absolutePath() };
}

[[nodiscard]] utils::error::Result<void> OSTreeRepo::push(const package::Reference &reference,
                                                          const std::string &module) const noexcept
{
    const auto defaultRepo = getDefaultRepo(this->cfg);
    const auto &remoteRepo = defaultRepo.name;
    std::string remoteURL;
    const auto &repo =
      std::find_if(this->cfg.repos.begin(), this->cfg.repos.end(), [&remoteRepo](const auto &repo) {
          return repo.alias.value_or(repo.name) == remoteRepo;
      });

    remoteURL = repo->url;

    return pushToRemote(remoteRepo, remoteURL, reference, module);
}

utils::error::Result<void> OSTreeRepo::pushToRemote(const std::string &remoteRepo,
                                                    const std::string &url,
                                                    const package::Reference &reference,
                                                    const std::string &module) const noexcept
{
    LINGLONG_TRACE("push " + reference.toString());
    auto layerDir = this->getLayerDir(reference, module);
    if (!layerDir) {
        return LINGLONG_ERR("layer not found", layerDir);
    }
    auto env = QProcessEnvironment::systemEnvironment();
    auto client = this->m_clientFactory.createClientV2();
    free(client->basePath); // NOLINT
    client->basePath = strdup(url.c_str());

    // 登录认证
    auto envUsername = env.value("LINGLONG_USERNAME").toUtf8();
    auto envPassword = env.value("LINGLONG_PASSWORD").toUtf8();
    request_auth_t auth;
    auth.username = envUsername.data();
    auth.password = envPassword.data();
    auto *signResRaw = ClientAPI_signIn(client.get(), &auth);
    if (signResRaw == nullptr) {
        return LINGLONG_ERR("sign error");
    }
    auto signRes = std::shared_ptr<sign_in_200_response_t>(signResRaw, sign_in_200_response_free);
    if (signRes->code != 200) {
        auto msg = signRes->msg ? signRes->msg : "cannot send request to remote server";
        return LINGLONG_ERR(QString("sign error(%1): %2").arg(auth.username).arg(msg));
    }
    auto *token = signRes->data->token;
    // 创建上传任务
    schema_new_upload_task_req_t newTaskReq;
    auto refStr = ostreeSpecFromReferenceV2(reference, std::nullopt, module);
    newTaskReq.ref = const_cast<char *>(refStr.c_str());
    newTaskReq.repo_name = const_cast<char *>(remoteRepo.c_str());
    auto *newTaskResRaw = ClientAPI_newUploadTaskID(client.get(), token, &newTaskReq);
    if (newTaskResRaw == nullptr) {
        return LINGLONG_ERR("create task error");
    }
    auto newTaskRes =
      std::shared_ptr<new_upload_task_id_200_response_t>(newTaskResRaw,
                                                         new_upload_task_id_200_response_free);
    if (newTaskRes->code != 200) {
        auto msg = newTaskRes->msg ? newTaskRes->msg : "cannot send request to remote server";
        return LINGLONG_ERR(QString("create task error: %1").arg(msg));
    }
    auto *taskID = newTaskRes->data->id;

    // 上传tar文件
    const QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        return LINGLONG_ERR(tmpDir.errorString());
    }

    const QString tarFileName = QString("%1.tgz").arg(reference.id);
    const QString tarFilePath = QDir::cleanPath(tmpDir.filePath(tarFileName));
    QStringList args = { "-zcf", tarFilePath, "-C", layerDir->absolutePath(), "." };
    auto tarStdout = utils::command::Exec("tar", args);
    if (!tarStdout) {
        return LINGLONG_ERR(tarStdout);
    }

    // 上传文件, 原来的binary_t需要将文件存储到内存，对大文件上传不友好，改为存储文件名
    // 底层改用 curl_mime_filedata 替换 curl_mime_data
    auto filepath = tarFilePath.toUtf8();
    auto filename = tarFileName.toUtf8();
    binary_t binary;
    binary.filepath = filepath.data();
    binary.filename = filename.data();
    auto *uploadTaskResRaw = ClientAPI_uploadTaskFile(client.get(), token, taskID, &binary);
    if (uploadTaskResRaw == nullptr) {
        return LINGLONG_ERR(QString("upload file error(%1)").arg(taskID));
    }
    auto uploadTaskRes =
      std::shared_ptr<api_upload_task_file_resp_t>(uploadTaskResRaw,
                                                   api_upload_task_file_resp_free);
    if (uploadTaskRes->code != 200) {
        auto msg = uploadTaskRes->msg ? uploadTaskRes->msg : "cannot send request to remote server";
        return LINGLONG_ERR(QString("upload file error(%1): %2").arg(taskID).arg(msg));
    }
    // 查询任务状态
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto *uploadInfoRaw = ClientAPI_uploadTaskInfo(client.get(), token, taskID);
        if (uploadInfoRaw == nullptr) {
            return LINGLONG_ERR(QString("get upload info error(%1)").arg(taskID));
        }
        auto uploadInfo =
          std::shared_ptr<upload_task_info_200_response_t>(uploadInfoRaw,
                                                           upload_task_info_200_response_free);
        if (uploadInfo->code != 200) {
            auto msg = uploadInfo->msg ? uploadInfo->msg : "cannot send request to remote server";
            return LINGLONG_ERR(QString("get upload info error(%1): %2").arg(taskID).arg(msg));
        }
        qInfo() << "pushing" << reference.toString() << module.c_str()
                << "status:" << uploadInfo->data->status;
        if (std::string(uploadInfo->data->status) == "complete") {
            return LINGLONG_OK;
        }
        if (std::string(uploadInfo->data->status) == "failed") {
            return LINGLONG_ERR(QString("An error occurred on the remote server(%1)").arg(taskID));
        }
    }
}

utils::error::Result<void> OSTreeRepo::remove(const package::Reference &ref,
                                              const std::string &module,
                                              const std::optional<std::string> &subRef) noexcept
{
    LINGLONG_TRACE("remove " + ref.toString());

    auto layer = this->getLayerItem(ref, module, subRef);
    if (!layer) {
        return LINGLONG_ERR(layer);
    }

    auto ret = this->removeOstreeRef(*layer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    auto layerDir = this->getLayerDir(*layer);
    if (!layerDir) {
        qWarning() << layerDir.error().message();
        return LINGLONG_OK;
    }

    if (!layerDir->removeRecursively()) {
        qCritical() << "Failed to remove dir: " << layerDir->absolutePath();
    }

    QFileInfo dirInfo{ layerDir->absolutePath() };
    if (!dirInfo.isSymLink()) {
        return LINGLONG_OK;
    }

    QDir target = dirInfo.symLinkTarget();
    QDir topLevel = dirInfo.absoluteDir().absolutePath();
    target.cdUp();
    while (topLevel.relativeFilePath(target.absolutePath()) != ".") {
        if (target.isEmpty() && !QFile::remove(target.absolutePath())) {
            qWarning() << "remove " << target.absolutePath() << "failed";
        }

        if (!target.cdUp()) {
            qCritical() << "failed to cd up from " << target.absolutePath();
            break;
        }
    }

    QFile::remove(layerDir->absolutePath());
    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::prune()
{
    LINGLONG_TRACE("prune ostree repo");
    // TODO(wurongjie) Perform pruning at the right time
    [[maybe_unused]] gint out_objects_total = 0;
    [[maybe_unused]] gint out_objects_pruned = 0;
    [[maybe_unused]] guint64 out_pruned_object_size_total = 0;
    g_autoptr(GError) gErr = nullptr;
    if (ostree_repo_prune(this->ostreeRepo.get(),
                          OSTREE_REPO_PRUNE_FLAGS_REFS_ONLY,
                          0,
                          &out_objects_total,
                          &out_objects_pruned,
                          &out_pruned_object_size_total,
                          nullptr,
                          &gErr)
        == FALSE) {
        return LINGLONG_ERR("ostree_repo_prune", gErr);
    }
    return LINGLONG_OK;
}

void OSTreeRepo::pull(service::PackageTask &taskContext,
                      const package::Reference &reference,
                      const std::string &module) noexcept
{
    // Note: if module is runtime, refString will be channel:id/version/binary.
    // because we need considering update channel:id/version/runtime to channel:id/version/binary.
    auto refString = ostreeSpecFromReferenceV2(reference, std::nullopt, module);
    LINGLONG_TRACE("pull " + QString::fromStdString(refString));

    utils::Transaction transaction;
    auto *cancellable = taskContext.cancellable();

    std::array<const char *, 2> refs{ refString.c_str(), nullptr };
    ostreeUserData data{ .taskContext = &taskContext };
    g_autoptr(OstreeAsyncProgress) progress =
      ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
    Q_ASSERT(progress != nullptr);

    g_autoptr(GError) gErr = nullptr;

    std::string userAgent = "linglong/" LINGLONG_VERSION;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "refs",
                          g_variant_new_variant(g_variant_new_strv(refs.data(), -1)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "append-user-agent",
                          g_variant_new_variant(g_variant_new_string(userAgent.c_str())));

    g_autoptr(GVariant) pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));
    // 这里不能使用g_main_context_push_thread_default，因为会阻塞Qt的事件循环
    const auto defaultRepo = getDefaultRepo(this->cfg);
    auto status = ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                                defaultRepo.name.c_str(),
                                                pull_options,
                                                progress,
                                                cancellable,
                                                &gErr);
    ostree_async_progress_finish(progress);
    auto shouldFallback = false;
    if (status == FALSE) {
        // gErr->code is 0, so we compare string here.
        if (!strstr(gErr->message, "No such branch")) {
            taskContext.reportError(LINGLONG_ERRV("ostree_repo_pull", gErr));
            return;
        }
        qWarning() << gErr->code << gErr->message;
        shouldFallback = true;
    }
    // Note: this fallback is only for binary to runtime
    if (shouldFallback && (module == "binary" || module == "runtime")) {
        g_autoptr(OstreeAsyncProgress) progress =
          ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
        Q_ASSERT(progress != nullptr);
        // fallback to old ref
        refString = ostreeSpecFromReference(reference, std::nullopt, module);
        qWarning() << "fallback to module runtime, pull " << QString::fromStdString(refString);

        refs[0] = refString.c_str();
        g_clear_error(&gErr);

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&builder,
                              "{s@v}",
                              "refs",
                              g_variant_new_variant(g_variant_new_strv(refs.data(), -1)));
        g_variant_builder_add(&builder,
                              "{s@v}",
                              "append-user-agent",
                              g_variant_new_variant(g_variant_new_string(userAgent.c_str())));

        g_autoptr(GVariant) pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));

        status = ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                               defaultRepo.name.c_str(),
                                               pull_options,
                                               progress,
                                               cancellable,
                                               &gErr);
        ostree_async_progress_finish(progress);
        if (status == FALSE) {
            taskContext.reportError(LINGLONG_ERRV("ostree_repo_pull", gErr));
            return;
        }
    }

    g_autofree char *commit = nullptr;
    g_autoptr(GFile) layerRootDir = nullptr;
    api::types::v1::RepositoryCacheLayersItem item;

    g_clear_error(&gErr);
    if (ostree_repo_read_commit(this->ostreeRepo.get(),
                                refs[0],
                                &layerRootDir,
                                &commit,
                                cancellable,
                                &gErr)
        == 0) {
        taskContext.reportError(LINGLONG_ERRV("ostree_repo_read_commit", gErr));
        return;
    }

    g_autoptr(GFile) infoFile = g_file_resolve_relative_path(layerRootDir, "info.json");
    auto info = utils::parsePackageInfo(infoFile);
    if (!info) {
        taskContext.reportError(LINGLONG_ERRV(info));
        return;
    }

    item.commit = commit;
    item.info = *info;
    item.repo = defaultRepo.name;

    auto layerDir = this->ensureEmptyLayerDir(item.commit);
    if (!layerDir) {
        taskContext.reportError(LINGLONG_ERRV(layerDir));
        return;
    }

    auto result = this->handleRepositoryUpdate(*layerDir, item);
    if (!result) {
        taskContext.reportError(LINGLONG_ERRV(result));
        return;
    }

    transaction.commit();
}

utils::error::Result<package::Reference>
OSTreeRepo::clearReference(const package::FuzzyReference &fuzzy,
                           const clearReferenceOption &opts,
                           const std::string &module) const noexcept
{
    LINGLONG_TRACE("clear fuzzy reference " + fuzzy.toString());

    utils::error::Result<package::Reference> reference = LINGLONG_ERR("reference not exists");

    if (!opts.forceRemote) {
        reference = clearReferenceLocal(*cache, fuzzy);
        if (reference) {
            return reference;
        }

        if (!opts.fallbackToRemote) {
            return LINGLONG_ERR(reference);
        }

        qInfo() << reference.error();
        qInfo() << "fallback to Remote";
    }

    auto list = this->listRemote(fuzzy);
    if (!list.has_value()) {
        return LINGLONG_ERR("get ref list from remote", list);
    }

    for (auto record : *list) {
        auto recordStr = nlohmann::json(record).dump();
        if (fuzzy.channel && fuzzy.channel->toStdString() != record.channel) {
            continue;
        }
        if (fuzzy.id.toStdString() != record.id) {
            continue;
        }
        auto version = package::Version::parse(QString::fromStdString(record.version));
        if (!version) {
            qWarning() << "Ignore invalid package record" << recordStr.c_str() << version.error();
            continue;
        }
        if (record.arch.empty()) {
            qWarning() << "Ignore invalid package record";
            continue;
        }
        if (module == "binary") {
            if (record.packageInfoV2Module != "binary" && record.packageInfoV2Module != "runtime") {
                continue;
            }
        } else {
            if (record.packageInfoV2Module != module) {
                continue;
            }
        }
        auto arch = package::Architecture::parse(record.arch[0]);
        if (!arch) {
            qWarning() << "Ignore invalid package record" << recordStr.c_str() << arch.error();
            continue;
        }
        auto channel = QString::fromStdString(record.channel);
        auto currentRef = package::Reference::create(channel, fuzzy.id, *version, *arch);
        if (!currentRef) {
            qWarning() << "Ignore invalid package record" << recordStr.c_str()
                       << currentRef.error();
            continue;
        }
        if (!reference) {
            reference = *currentRef;
            continue;
        }

        if (reference->version >= currentRef->version) {
            continue;
        }

        reference = *currentRef;
    }

    if (!reference) {
        auto msg = QString("not found ref:%1 module:%2 from remote repo")
                     .arg(fuzzy.toString())
                     .arg(module.c_str());
        return LINGLONG_ERR(msg);
    }

    return reference;
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listLocal() const noexcept
{
    std::vector<api::types::v1::PackageInfoV2> pkgInfos;

    QDir layersDir = this->repoDir.absoluteFilePath("layers");
    Q_ASSERT(layersDir.exists());

    auto items = this->cache->queryExistingLayerItem();
    pkgInfos.reserve(items.size());
    for (const auto &item : items) {
        if (item.deleted && item.deleted.value()) {
            continue;
        }

        pkgInfos.emplace_back(item.info);
    }

    return pkgInfos;
}

// Note: Since version 1.7.0, multiple versions are no longer supported, so there will be multiple
// versions locally. In some places, we need to list the latest version of each package.
utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listLocalLatest() const noexcept
{
    LINGLONG_TRACE("list local latest package");
    std::vector<api::types::v1::PackageInfoV2> pkgInfos;

    QDir layersDir = this->repoDir.absoluteFilePath("layers");
    Q_ASSERT(layersDir.exists());

    auto items = this->cache->queryExistingLayerItem();
    pkgInfos.reserve(items.size());
    for (const auto &item : items) {
        if (item.deleted && item.deleted.value()) {
            continue;
        }

        auto it = std::find_if(pkgInfos.begin(),
                               pkgInfos.end(),
                               [&item](const api::types::v1::PackageInfoV2 &info) {
                                   return item.info.id == info.id;
                               });
        if (it == pkgInfos.end()) {
            pkgInfos.emplace_back(item.info);
            continue;
        }

        auto pkgInfoVersion = package::Version::parse(QString::fromStdString(it->version));
        if (!pkgInfoVersion) {
            return LINGLONG_ERR(pkgInfoVersion);
        }
        auto itemVersion = package::Version::parse(QString::fromStdString(item.info.version));
        if (!itemVersion) {
            return LINGLONG_ERR(itemVersion);
        }

        if (*itemVersion <= *pkgInfoVersion) {
            continue;
        }
        pkgInfos.erase(it);
        pkgInfos.emplace_back(item.info);
    }

    return pkgInfos;
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listRemote(const package::FuzzyReference &fuzzyRef) const noexcept
{
    LINGLONG_TRACE("list remote references");

    auto client = m_clientFactory.createClientV2();
    request_fuzzy_search_req_t req{ nullptr, nullptr, nullptr, nullptr, nullptr };
    auto freeIfNotNull = utils::finally::finally([&req] {
        if (req.app_id != nullptr) {
            free(req.app_id); // NOLINT
        }
        if (req.channel != nullptr) {
            free(req.channel); // NOLINT
        }
        if (req.version != nullptr) {
            free(req.version); // NOLINT
        }
        if (req.arch != nullptr) {
            free(req.arch); // NOLINT
        }
        if (req.repo_name != nullptr) {
            free(req.repo_name); // NOLINT
        }
    });

    auto id = fuzzyRef.id.toLatin1();
    req.app_id = ::strndup(id.data(), id.size());
    if (req.app_id == nullptr) {
        return LINGLONG_ERR(QString{ "strndup app_id failed: %1" }.arg(fuzzyRef.id));
    }
    const auto defaultRepo = getDefaultRepo(this->cfg);
    req.repo_name = ::strndup(defaultRepo.name.data(), defaultRepo.name.size());
    if (req.repo_name == nullptr) {
        return LINGLONG_ERR(
          QString{ "strndup repo_name failed: %1" }.arg(defaultRepo.name.c_str()));
    }

    if (fuzzyRef.channel) {
        auto channel = fuzzyRef.channel->toLatin1();
        req.channel = strndup(channel.data(), channel.size());
        if (req.channel == nullptr) {
            return LINGLONG_ERR(QString{ "strndup channel failed: %1" }.arg(channel.data()));
        }
    }

    if (fuzzyRef.version) {
        auto version = fuzzyRef.version->toString().toLatin1();
        req.version = strndup(version.data(), version.size());
        if (req.version == nullptr) {
            return LINGLONG_ERR(QString{ "strndup version failed: %1" }.arg(version.data()));
        }
    }

    auto defaultArch = package::Architecture::currentCPUArchitecture();
    if (!defaultArch) {
        return LINGLONG_ERR(defaultArch);
    }

    auto arch = fuzzyRef.arch.value_or(*defaultArch);
    auto archStr = arch.toString().toLatin1();
    req.arch = strndup(archStr.data(), archStr.size());
    if (req.arch == nullptr) {
        return LINGLONG_ERR(QString{ "strndup arch failed: %1" }.arg(archStr.data()));
    }

    // wait http request to finish
    fuzzy_search_app_200_response_t *response{ nullptr };
    QEventLoop loop;
    auto job = std::thread(
      [client, &loop, &response](request_fuzzy_search_req_t req) {
          response = ClientAPI_fuzzySearchApp(client.get(), &req);
          loop.exit();
      },
      req);
    // transfer ownership
    req.app_id = nullptr;
    req.channel = nullptr;
    req.version = nullptr;
    req.arch = nullptr;
    req.repo_name = nullptr;

    loop.exec();
    if (job.joinable()) {
        job.join();
    }

    if (response == nullptr) {
        return LINGLONG_ERR("failed to send request to remote server");
    }
    auto freeResponse = utils::finally::finally([&response] {
        fuzzy_search_app_200_response_free(response);
    });

    if (response->code != 200) {
        QString msg = (response->msg != nullptr)
          ? response->msg
          : QString{ "cannot send request to remote server: %1" }.arg(response->code);
        return LINGLONG_ERR(msg);
    }

    if (response->data == nullptr) {
        return {};
    }

    std::vector<api::types::v1::PackageInfoV2> pkgInfos;
    pkgInfos.reserve(response->data->count);
    for (auto *entry = response->data->firstEntry; entry != nullptr; entry = entry->nextListEntry) {
        auto *item = (request_register_struct_t *)entry->data;
        pkgInfos.emplace_back(api::types::v1::PackageInfoV2{
          .arch = { item->arch },
          .channel = item->channel,
          .description = item->description,
          .id = item->app_id,
          .kind = item->kind,
          .packageInfoV2Module = item->module,
          .name = item->name,
          .runtime = item->runtime,
          .size = item->size,
          .version = item->version,
        });
    }

    return pkgInfos;
}

void OSTreeRepo::removeDanglingXDGIntergation() noexcept
{
    QDir entriesDir = this->repoDir.absoluteFilePath("entries/share");
    QDirIterator it(entriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDot | QDir::NoDotDot | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto info = it.fileInfo();
        if (info.isDir()) {
            continue;
        }

        if (!info.isSymLink()) {
            // NOTE: Everything in entries should be directory or symbol link.
            // But it can be some cache file, we should not remove it too.
            qWarning() << "Invalid file detected." << info.absoluteFilePath();
            qWarning() << "If the file is a cache or something like that, ignore this warning.";
            continue;
        }

        if (info.exists()) {
            continue;
        }

        if (!entriesDir.remove(it.filePath())) {
            qCritical() << "Failed to remove" << it.filePath();
            Q_ASSERT(false);
        }
    }
    this->updateSharedInfo();
}

void OSTreeRepo::unexportReference(const package::Reference &ref) noexcept
{
    auto layerDir = this->getLayerDir(ref);
    if (!layerDir) {
        qCritical() << "Failed to unexport" << ref.toString() << layerDir.error().message();
        return;
    }

    QDir entriesDir = this->repoDir.absoluteFilePath("entries");
    QDirIterator it(entriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDot | QDir::NoDotDot | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto info = it.fileInfo();
        if (info.isDir()) {
            continue;
        }

        if (!info.isSymLink()) {
            // NOTE: Everything in entries should be directory or symbol link.
            // But it can be some cache file, we should not remove it too.
            qWarning() << "Invalid file detected." << info.absoluteFilePath();
            qWarning() << "If the file is a cache or something like that, ignore this warning.";
            continue;
        }

        if (!info.symLinkTarget().startsWith(layerDir->absolutePath())) {
            continue;
        }

        if (!entriesDir.remove(it.filePath())) {
            qCritical() << "Failed to remove" << it.filePath();
            Q_ASSERT(false);
        }
    }
    this->updateSharedInfo();
}

void OSTreeRepo::exportReference(const package::Reference &ref) noexcept
{
    auto entriesDir = QDir(this->repoDir.absoluteFilePath("entries"));
    if (!entriesDir.exists()) {
        entriesDir.mkpath(".");
    }
    auto item = this->getLayerItem(ref);
    if (!item.has_value()) {
        qCritical() << QString("Failed to export %1:").arg(ref.toString())
                    << "layer directory not exists." << item.error().message();
        Q_ASSERT(false);
        return;
    }
    auto ret = exportEntries(entriesDir.absolutePath().toStdString(), *item);
    if (!ret.has_value()) {
        qCritical() << QString("Failed to export %1:").arg(ref.toString()) << ret.error().message();
        Q_ASSERT(false);
        return;
    }
    this->updateSharedInfo();
}

// 递归源目录所有文件，并在目标目录创建软链接，max_depth 控制递归深度以避免环形链接导致的无限递归
utils::error::Result<void> OSTreeRepo::exportDir(const std::string &appID,
                                                 const std::filesystem::path &source,
                                                 const std::filesystem::path &destination,
                                                 const int &max_depth)
{
    LINGLONG_TRACE(QString("export %1").arg(source.c_str()));
    if (max_depth <= 0) {
        qWarning() << "ttl reached, skipping export for" << source.c_str();
        return LINGLONG_OK;
    }

    std::error_code ec;
    // 检查源目录是否存在
    auto exists = std::filesystem::exists(source, ec);
    if (ec) {
        return LINGLONG_ERR("check source", ec);
    }
    if (!exists) {
        return LINGLONG_ERR("source directory does not exist");
    }
    auto is_directory = std::filesystem::is_directory(source, ec);
    if (ec) {
        return LINGLONG_ERR("check source", ec);
    }
    if (!is_directory) {
        return LINGLONG_ERR("source is not a directory");
    }
    // 检查目标目录是否存在，如果不存在则创建
    exists = std::filesystem::exists(destination, ec);
    if (ec) {
        return LINGLONG_ERR(QString("Failed to check file existence: ") + destination.c_str(), ec);
    }
    // 如果目标非目录，则删除它并重新创建
    if (exists && !std::filesystem::is_directory(destination, ec)) {
        std::filesystem::remove(destination, ec);
        if (ec) {
            return LINGLONG_ERR(QString("Failed to remove file: ") + destination.c_str(), ec);
        }
        // 标记目标不存在
        exists = false;
    }
    if (!exists) {
        std::filesystem::create_directories(destination, ec);
        if (ec) {
            return LINGLONG_ERR(QString("Failed to create directory: ") + destination.c_str(), ec);
        }
    }
    auto iterator = std::filesystem::directory_iterator(source, ec);
    if (ec) {
        return LINGLONG_ERR("list directory: " + source.string(), ec);
    }
    // 遍历源目录中的所有文件和子目录
    for (const auto &entry : iterator) {
        const auto &source_path = entry.path();
        const auto &target_path = destination / source_path.filename();
        // 跳过无效的软链接
        exists = std::filesystem::exists(source_path, ec);
        if (ec) {
            return LINGLONG_ERR("check source existence" + source_path.string(), ec);
        }
        if (!exists) {
            continue;
        }

        // 如果是文件，创建符号链接
        auto is_regular_file = std::filesystem::is_regular_file(source_path, ec);
        if (ec) {
            return LINGLONG_ERR("check file type: " + source_path.string(), ec);
        }
        if (is_regular_file) {
            exists = std::filesystem::exists(target_path, ec);
            if (ec) {
                return LINGLONG_ERR("check file existence", ec);
            }
            if (exists) {
                std::filesystem::remove(target_path, ec);
                if (ec) {
                    return LINGLONG_ERR("remove file failed", ec);
                }
            }

            {
                auto info = QFileInfo(target_path.c_str());
                if ((info.path().contains("share/applications") && info.suffix() == "desktop")
                    || (info.path().contains("share/dbus-1") && info.suffix() == "service")
                    || (info.path().contains("share/systemd/user") && info.suffix() == "service")
                    || (info.path().contains("share/applications/context-menus"))) {
                    // We should not modify the files of the checked application directly, but
                    // should copy them and then modify.
                    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                    auto sourceNewPath = QString{ "%1/%2_%3.%4" }
                                           .arg((QFileInfo(source_path.c_str()).absolutePath()))
                                           .arg(info.baseName())
                                           .arg(QString::fromStdString(std::to_string(timestamp)))
                                           .arg(info.suffix())
                                           .toStdString();
                    std::filesystem::copy(source_path, sourceNewPath, ec);
                    if (ec) {
                        return LINGLONG_ERR("copy file failed: " + sourceNewPath, ec);
                    }

                    exists = std::filesystem::exists(sourceNewPath, ec);
                    if (ec) {
                        return LINGLONG_ERR("check file exists", ec);
                    }

                    if (!exists) {
                        qWarning() << "failed to copy file: " << sourceNewPath.c_str();
                        continue;
                    }

                    auto ret = IniLikeFileRewrite(QFileInfo(sourceNewPath.c_str()), appID.c_str());
                    if (ret) {
                        std::filesystem::rename(sourceNewPath, source_path, ec);
                        if (ec) {
                            return LINGLONG_ERR("rename new path", ec);
                        }
                    } else {
                        qWarning() << "rewrite file failed: " << ret.error().message();
                    }
                }

                // 此处应该采用相对路径创建软链接，采用绝对路径对于某些应用(帮助手册）在容器里面是无法访问的
                std::filesystem::create_symlink(
                  source_path.lexically_relative(target_path.parent_path()),
                  target_path,
                  ec);
                if (ec) {
                    return LINGLONG_ERR("create symlink failed: " + target_path.string(), ec);
                }
            }

            continue;
        }

        // 如果是目录，进行递归导出
        is_directory = std::filesystem::is_directory(source_path, ec);
        if (ec) {
            return LINGLONG_ERR("check file type", ec);
        }
        if (is_directory) {
            auto ret = this->exportDir(appID, source_path, target_path, max_depth - 1);
            if (!ret.has_value()) {
                return ret;
            }
            continue;
        }
        // 其他情况，报错
        qWarning() << "invalid file: " << source_path.c_str();
    }
    return LINGLONG_OK;
}

utils::error::Result<void>
OSTreeRepo::exportEntries(const std::filesystem::path &rootEntriesDir,
                          const api::types::v1::RepositoryCacheLayersItem &item) noexcept
{
    LINGLONG_TRACE(QString("export %1").arg(item.info.id.c_str()));
    auto layerDir = getMergedModuleDir(item);
    if (!layerDir.has_value()) {
        return LINGLONG_ERR("get layer dir", layerDir);
    }
    std::error_code ec;
    // 检查目录是否存在
    std::filesystem::path appEntriesDir = layerDir->absoluteFilePath("entries").toStdString();
    auto exists = std::filesystem::exists(appEntriesDir, ec);
    if (ec) {
        return LINGLONG_ERR("check appEntriesDir exists", ec);
    }
    if (!exists) {
        qCritical() << QString("Failed to export %1:").arg(item.info.id.c_str())
                    << appEntriesDir.c_str() << "not exists.";
        return LINGLONG_OK;
    }
    std::vector<std::string> exportPaths = {
        "share/applications", // Copy desktop files
        "share/mime",         // Copy MIME Type files
        "share/icons",        // Icons
        "share/dbus-1",       // D-Bus service files
        "share/gnome-shell",  // Search providers
        "share/appdata",      // Copy appdata/metainfo files (legacy path)
        "share/metainfo",     // Copy appdata/metainfo files
        "share/plugins", // Copy plugins conf，The configuration files provided by some applications
                         // maybe used by the host dde-file-manager.
        "share/deepin-manual",     // copy deepin-manual files
        "share/deepin-elf-verify", // for uab signature

    };
    // 如果存在lib/systemd目录，则导出lib/systemd，否则导出share/systemd
    exists = std::filesystem::exists(appEntriesDir / "lib/systemd", ec);
    if (ec) {
        return LINGLONG_ERR("Failed to check the existence of lib/systemd directory: {}", ec);
    }
    if (exists) {
        exportPaths.push_back("lib/systemd");
    } else {
        exportPaths.push_back("share/systemd");
    }
    // 导出应用entries目录下的所有文件到玲珑仓库的entries目录下
    for (const auto &path : exportPaths) {
        auto source = appEntriesDir / path;
        auto destination = rootEntriesDir / path;
        // 将 share/systemd 目录下的文件导出到 lib/systemd 目录下
        if (path == "share/systemd") {
            destination = rootEntriesDir / "lib/systemd";
        }
        // 检查源目录是否存在，跳过不存在的目录
        exists = std::filesystem::exists(source, ec);
        if (ec) {
            return LINGLONG_ERR(QString("Failed to check file existence: ") + source.c_str(), ec);
        }
        if (!exists) {
            continue;
        }
        auto ret = this->exportDir(item.info.id, source, destination, 10);
        if (!ret.has_value()) {
            return ret;
        }
    }
    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::fixExportAllEntries() noexcept
{
    auto exportVersion = repoDir.absoluteFilePath("entries/.version").toStdString();
    auto data = linglong::utils::readFile(exportVersion);
    if (data && data == LINGLONG_EXPORT_VERSION) {
        qDebug() << exportVersion.c_str() << data->c_str();
        qDebug() << "skip export entry, already exported";
    } else {
        auto ret = exportAllEntries();
        if (!ret.has_value()) {
            qCritical() << "failed to export entries:" << ret.error();
            return ret;
        } else {
            ret = linglong::utils::writeFile(exportVersion, LINGLONG_EXPORT_VERSION);
            if (!ret.has_value()) {
                qCritical() << "failed to write export version:" << ret.error();
                return ret;
            }
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::exportAllEntries() noexcept
{
    LINGLONG_TRACE("export all entries");
    std::error_code ec;
    // 创建一个新的entries目录，使用UUID作为名称
    auto id = QUuid::createUuid().toString(QUuid::Id128);
    std::filesystem::path entriesDir = this->repoDir.filePath("entries_new_" + id).toStdString();
    std::filesystem::create_directory(entriesDir, ec);
    if (ec) {
        return LINGLONG_ERR("create temp share directory", ec);
    }
    // 导出所有layer到新entries目录
    auto items = this->cache->queryExistingLayerItem();
    for (const auto &item : items) {
        if (item.info.kind != "app") {
            continue;
        }
        auto ret = exportEntries(entriesDir, item);
        if (!ret.has_value()) {
            return ret;
        }
    }
    // 用新的entries目录替换旧的
    std::filesystem::path workdir = repoDir.absolutePath().toStdString();
    auto existsOldEntries = std::filesystem::exists(workdir / "entries", ec);
    if (ec) {
        return LINGLONG_ERR("check entries directory", ec);
    }
    if (!existsOldEntries) {
        std::filesystem::rename(entriesDir, workdir / "entries", ec);
        if (ec) {
            return LINGLONG_ERR("rename new entries directory", ec);
        }
    } else {
        auto id = QUuid::createUuid().toString(QUuid::Id128).toStdString();
        auto oldEntriesDir = workdir / ("entries_old_" + id);
        std::filesystem::rename(workdir / "entries", oldEntriesDir, ec);
        if (ec) {
            return LINGLONG_ERR("rename old share directory", ec);
        }
        std::filesystem::rename(entriesDir, workdir / "entries", ec);
        if (ec) {
            return LINGLONG_ERR("create new share symlink", ec);
        }
        std::filesystem::remove_all(oldEntriesDir, ec);
        if (ec) {
            return LINGLONG_ERR("remove old share directory", ec);
        }
    }
    this->updateSharedInfo();
    return LINGLONG_OK;
}

void OSTreeRepo::updateSharedInfo() noexcept
{
    LINGLONG_TRACE("update shared info");

    auto applicationDir = QDir(this->repoDir.absoluteFilePath("entries/share/applications"));
    auto mimeDataDir = QDir(this->repoDir.absoluteFilePath("entries/share/mime"));
    auto glibSchemasDir = QDir(this->repoDir.absoluteFilePath("entries/share/glib-2.0/schemas"));
    // 更新 desktop database
    if (applicationDir.exists()) {
        auto ret =
          utils::command::Exec("update-desktop-database", { applicationDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update desktop database in "
                + applicationDir.absolutePath() + ": " + ret.error().message();
        }
    }

    // 更新 mime type database
    if (mimeDataDir.exists()) {
        auto ret = utils::command::Exec("update-mime-database", { mimeDataDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update mime type database in "
                + mimeDataDir.absolutePath() + ": " + ret.error().message();
        }
    }

    // 更新 glib-2.0/schemas
    if (glibSchemasDir.exists()) {
        auto ret = utils::command::Exec("glib-compile-schemas", { glibSchemasDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update schemas in " + glibSchemasDir.absolutePath()
                + ": " + ret.error().message();
        }
    }
}

utils::error::Result<void>
OSTreeRepo::markDeleted(const package::Reference &ref,
                        bool deleted,
                        const std::string &module,
                        const std::optional<std::string> &subRef) noexcept
{
    LINGLONG_TRACE("mark " + ref.toString() + " to deleted");

    auto item = this->getLayerItem(ref, module, subRef);
    if (!item) {
        return LINGLONG_ERR(item);
    }

    auto it = this->cache->findMatchingItem(*item);
    if (!it) {
        return LINGLONG_ERR(it);
    }

    auto originalValue = (*it)->deleted;
    std::optional<bool> deletedOpt = deleted ? std::optional<bool>(true) : std::nullopt;

    utils::Transaction transaction;
    (*it)->deleted = deletedOpt;
    transaction.addRollBack([iterator = *it, originalValue]() noexcept {
        iterator->deleted = originalValue;
    });

    auto result = this->cache->writeToDisk();
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
OSTreeRepo::getLayerItem(const package::Reference &ref,
                         std::string module,
                         const std::optional<std::string> &subRef) const noexcept
{
    LINGLONG_TRACE("get latest layer of " + ref.toString());
    if (module == "runtime") {
        module = "binary";
    }

    repoCacheQuery query{ .id = ref.id.toStdString(),
                          .repo = std::nullopt,
                          .channel = ref.channel.toStdString(),
                          .version = ref.version.toString().toStdString(),
                          .module = module,
                          .uuid = subRef,
                          .deleted = std::nullopt };
    auto items = this->cache->queryLayerItem(query);
    auto count = items.size();
    if (count > 1) {
        std::for_each(items.begin(),
                      items.end(),
                      [](const api::types::v1::RepositoryCacheLayersItem &item) {
                          qDebug().nospace()
                            << "dump item ref [" << item.repo.c_str() << ":" << item.info.id.c_str()
                            << ":" << item.info.version.c_str() << ":"
                            << item.info.arch.front().c_str() << ":"
                            << item.info.packageInfoV2Module.c_str() << "]";
                      });
        return LINGLONG_ERR("ambiguous ref has been detected, maybe underlying storage already "
                            "broken.");
    }

    if (count == 0) {
        if (query.module != "binary") {
            return LINGLONG_ERR("couldn't find layer item " % ref.toString() % "/"
                                % module.c_str());
        }

        qDebug() << "fallback to runtime:" << query.to_string().c_str();
        query.module = "runtime";
        items = this->cache->queryLayerItem(query);
        if (items.size() > 1) {
            return LINGLONG_ERR("ambiguous ref has been detected, maybe underlying storage already "
                                "broken.");
        }

        if (items.size() == 0) {
            return LINGLONG_ERR(ref.toString() + " fallback to runtime still not found");
        }
    }

    return items.front();
}

auto OSTreeRepo::getLayerDir(const api::types::v1::RepositoryCacheLayersItem &layer) const noexcept
  -> utils::error::Result<package::LayerDir>
{
    LINGLONG_TRACE("get dir from layer item "
                   + QString::fromStdString(ostreeRefSpecFromLayerItem(layer)));

    QDir dir = this->repoDir.absoluteFilePath(QString::fromStdString("layers/" + layer.commit));
    if (!dir.exists()) {
        return LINGLONG_ERR(dir.absolutePath() + " doesn't exist");
    }
    return dir.absolutePath();
}

auto OSTreeRepo::getLayerDir(const package::Reference &ref,
                             const std::string &module,
                             const std::optional<std::string> &subRef) const noexcept
  -> utils::error::Result<package::LayerDir>
{
    LINGLONG_TRACE("get dir from ref " + ref.toString());

    auto layer = this->getLayerItem(ref, module, subRef);
    if (!layer) {
        qDebug().nospace() << "no such item:" << ref.toString() << "/" << module.c_str() << ":"
                           << layer.error().message();
        return LINGLONG_ERR(layer);
    }

    return getLayerDir(*layer);
}

// get all module list
utils::error::Result<std::vector<std::string>> OSTreeRepo::getRemoteModuleList(
  const package::Reference &ref, const std::optional<std::vector<std::string>> &filter) noexcept
{
    LINGLONG_TRACE("get remote module list");
    auto fuzzy = package::FuzzyReference::create(ref.channel, ref.id, ref.version, ref.arch);
    if (!fuzzy.has_value()) {
        return LINGLONG_ERR("create fuzzy reference", fuzzy);
    }
    auto list = this->listRemote(*fuzzy);
    if (!list.has_value()) {
        return LINGLONG_ERR("list remote reference", fuzzy);
    }
    if (list->size() == 0) {
        return {};
    }
    auto include = [](const std::vector<std::string> &arr, const std::string &item) {
        return std::find(arr.begin(), arr.end(), item) != arr.end();
    };
    std::vector<std::string> modules;
    for (const auto &ref : *list) {
        auto remoteModule = ref.packageInfoV2Module;
        // 如果不筛选，返回所有module
        if (!filter.has_value()) {
            modules.push_back(remoteModule);
            continue;
        }
        // 如果筛选，只返回指定的module
        if (include(filter.value(), remoteModule)) {
            modules.push_back(remoteModule);
            continue;
        }
        // TODO 在未来删除对旧版本runtime module的兼容

        // 如果过滤列表包含runtime，可以使用binary替换
        // 这一般是在升级时，本地runtime模块升级到binary模块
        if (remoteModule == "binary" && include(filter.value(), "runtime")) {
            modules.push_back(remoteModule);
            continue;
        }
    }
    // 如果想安装binary模块，但远程没有binary模块，就安装runtime模块
    if (include(filter.value(), "binary") && !include(modules, "binary")) {
        modules.push_back("runtime");
    }
    std::sort(modules.begin(), modules.end());
    auto it = std::unique(modules.begin(), modules.end());
    modules.erase(it, modules.end());
    return modules;
}

// get all module list
std::vector<std::string> OSTreeRepo::getModuleList(const package::Reference &ref) noexcept
{
    repoCacheQuery query{
        .id = ref.id.toStdString(),
        .repo = std::nullopt,
        .channel = ref.channel.toStdString(),
        .version = ref.version.toString().toStdString(),
    };
    auto layers = this->cache->queryLayerItem(query);
    // 按module字母从小到大排序，提前排序以保证后面的commits比较
    std::sort(layers.begin(),
              layers.end(),
              [](api::types::v1::RepositoryCacheLayersItem lhs,
                 api::types::v1::RepositoryCacheLayersItem rhs) {
                  return lhs.info.packageInfoV2Module < rhs.info.packageInfoV2Module;
              });
    std::vector<std::string> modules;
    for (const auto &item : layers) {
        modules.push_back(item.info.packageInfoV2Module);
    }
    return modules;
}

// 获取合并后的layerDir，如果没有找到则返回binary模块的layerDir
utils::error::Result<package::LayerDir>
OSTreeRepo::getMergedModuleDir(const package::Reference &ref, bool fallbackLayerDir) const noexcept
{
    LINGLONG_TRACE("get merge dir from ref " + ref.toString());
    qDebug() << "getMergedModuleDir" << ref.toString();
    QDir mergedDir = this->repoDir.absoluteFilePath("merged");
    auto layer = this->getLayerItem(ref, "binary", {});
    if (!layer) {
        qDebug().nospace() << "no such item:" << ref.toString()
                           << "/binary:" << layer.error().message();
        return LINGLONG_ERR(layer);
    }
    return getMergedModuleDir(*layer, fallbackLayerDir);
}

// 获取合并后的layerDir，如果没有找到则返回binary模块的layerDir
utils::error::Result<package::LayerDir> OSTreeRepo::getMergedModuleDir(
  const api::types::v1::RepositoryCacheLayersItem &layer, bool fallbackLayerDir) const noexcept
{
    LINGLONG_TRACE("get merge dir from layer " + QString::fromStdString(layer.info.id));
    QDir mergedDir = this->repoDir.absoluteFilePath("merged");
    auto items = this->cache->queryMergedItems();
    // 如果没有merged记录，尝试使用layer
    if (!items.has_value()) {
        qDebug().nospace() << "not exists merged items";
        if (fallbackLayerDir) {
            return getLayerDir(layer);
        }
        return LINGLONG_ERR("no merged item found");
    }
    // 如果找到layer对应的merge，就返回merge目录，否则回退到layer目录
    for (auto item : items.value()) {
        if (item.binaryCommit == layer.commit) {
            QDir dir = mergedDir.filePath(item.id.c_str());
            if (dir.exists()) {
                return dir.path();
            } else {
                qWarning().nospace() << "not exists merged dir" << dir;
            }
        }
    }
    if (fallbackLayerDir) {
        return getLayerDir(layer);
    }
    return LINGLONG_ERR("merged doesn't exist");
}

utils::error::Result<std::shared_ptr<package::LayerDir>> OSTreeRepo::getMergedModuleDir(
  const package::Reference &ref, const QStringList &loadModules) const noexcept
{
    LINGLONG_TRACE("merge modules");
    QDir mergedDir = this->repoDir.absoluteFilePath("merged");
    auto layerItems = this->cache->queryExistingLayerItem();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::vector<std::string> commits;
    std::string findModules;
    // 筛选指定的layer
    for (auto &layer : layerItems) {
        std::string arch;
        if (!layer.info.arch.empty()) {
            arch = layer.info.arch.front();
        }
        if (layer.info.id != ref.id.toStdString()
            || layer.info.version != ref.version.toString().toStdString()
            || arch != ref.arch.toString().toStdString()) {
            continue;
        }
        if (!loadModules.contains(layer.info.packageInfoV2Module.c_str())) {
            continue;
        }
        commits.push_back(layer.commit);
        findModules += layer.info.packageInfoV2Module + " ";
        hash.addData(QString::fromStdString(layer.commit).toUtf8());
    }
    if (commits.empty()) {
        return LINGLONG_ERR("not found any layer");
    }
    // 模块未全部找到
    if (commits.size() < loadModules.size()) {
        return LINGLONG_ERR(QString("missing module, only found: ") + findModules.c_str());
    }
    // 合并layer，生成临时merged目录
    QString mergeID = hash.result().toHex();
    auto mergeTmp = mergedDir.filePath("tmp_" + mergeID);
    for (auto commit : commits) {
        int root = open("/", O_DIRECTORY);
        auto _ = utils::finally::finally([root]() {
            close(root);
        });
        g_autoptr(GError) gErr = nullptr;
        OstreeRepoCheckoutAtOptions opt = {};
        opt.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_ADD_FILES;
        if (ostree_repo_checkout_at(this->ostreeRepo.get(),
                                    &opt,
                                    root,
                                    mergeTmp.mid(1).toUtf8(),
                                    commit.c_str(),
                                    nullptr,
                                    &gErr)
            == FALSE) {
            return LINGLONG_ERR(QString("ostree_repo_checkout_at %1").arg(mergeTmp), gErr);
        }
    }
    auto ptr = new package::LayerDir(mergeTmp);
    return std::shared_ptr<package::LayerDir>(ptr, [](package::LayerDir *ptr) {
        ptr->removeRecursively();
        delete ptr;
    });
}

utils::error::Result<void> OSTreeRepo::mergeModules() const noexcept
{
    LINGLONG_TRACE("merge modules");
    std::error_code ec;
    QDir mergedDir = this->repoDir.absoluteFilePath("merged");
    mergedDir.mkpath(".");
    auto layerItems = this->cache->queryExistingLayerItem();
    auto mergedItems = this->cache->queryMergedItems();
    // 对layerItems分组
    std::map<std::string, std::vector<api::types::v1::RepositoryCacheLayersItem>> layerGroup;
    for (auto &layer : layerItems) {
        std::string arch;
        if (!layer.info.arch.empty()) {
            arch = layer.info.arch.front();
        }
        // 将id、version和arch相同的item合并，不区分repo和channel
        auto groupKey = QString("%1/%2/%3")
                          .arg(layer.info.id.c_str())
                          .arg(layer.info.version.c_str())
                          .arg(arch.c_str())
                          .toStdString();
        layerGroup[groupKey].push_back(layer);
    }

    // 对同组layer进行合并，生成mergedItem
    std::vector<api::types::v1::RepositoryCacheMergedItem> newMergedItems;
    for (auto &it : layerGroup) {
        auto &layers = it.second;
        // 只有一个module不需要合并
        if (layers.size() == 1) {
            continue;
        }
        // 按module字母从小到大排序，提前排序以保证后面的commits比较
        std::sort(layers.begin(),
                  layers.end(),
                  [](api::types::v1::RepositoryCacheLayersItem lhs,
                     api::types::v1::RepositoryCacheLayersItem rhs) {
                      return lhs.info.packageInfoV2Module < rhs.info.packageInfoV2Module;
                  });
        // 查找binary模块的commit id
        std::string binaryCommit;
        std::vector<std::string> commits;
        std::vector<std::string> modules;
        QCryptographicHash hash(QCryptographicHash::Sha256);
        for (const auto &layer : layers) {
            commits.push_back(layer.commit);
            modules.push_back(layer.info.packageInfoV2Module);
            hash.addData(QString::fromStdString(layer.commit).toUtf8());
            if (layer.info.packageInfoV2Module == "binary") {
                binaryCommit = layer.commit;
            }
        }
        if (binaryCommit.empty()) {
            continue;
        }
        auto mergeID = hash.result().toHex().toStdString();
        // 判断单个merged是否有变动
        auto mergedChanged = true;
        if (mergedItems.has_value()) {
            // 查找已存在的merged记录
            for (auto &merge : mergedItems.value()) {
                if (merge.id == mergeID) {
                    if (merge.commits == commits) {
                        newMergedItems.push_back(merge);
                        mergedChanged = false;
                    }
                    break;
                }
            }
        }
        if (!mergedChanged) {
            continue;
        }
        // 创建临时目录
        auto mergeTmp = mergedDir.filePath(QString("tmp_") + mergeID.c_str());
        std::filesystem::remove_all(mergeTmp.toStdString(), ec);
        if (ec) {
            return LINGLONG_ERR("clean merge tmp dir", ec);
        }
        std::filesystem::create_directories(mergeTmp.toStdString(), ec);
        if (ec) {
            return LINGLONG_ERR("create merge tmp dir", ec);
        }
        // 将所有module文件合并到临时目录
        for (const auto &layer : layers) {
            qDebug() << "merge module" << it.first.c_str()
                     << layer.info.packageInfoV2Module.c_str();
            int root = open("/", O_DIRECTORY);
            auto _ = utils::finally::finally([root]() {
                close(root);
            });
            g_autoptr(GError) gErr = nullptr;
            OstreeRepoCheckoutAtOptions opt = {};
            opt.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_ADD_FILES;
            if (ostree_repo_checkout_at(this->ostreeRepo.get(),
                                        &opt,
                                        root,
                                        mergeTmp.mid(1).toUtf8(),
                                        layer.commit.c_str(),
                                        nullptr,
                                        &gErr)
                == FALSE) {
                return LINGLONG_ERR(QString("ostree_repo_checkout_at %1").arg(mergeTmp), gErr);
            }
        }
        // 将临时目录改名到正式目录，以binary模块的commit为文件名
        auto mergeOutput = mergedDir.filePath(mergeID.c_str());
        std::filesystem::remove_all(mergeOutput.toStdString(), ec);
        if (ec) {
            return LINGLONG_ERR("clean merge dir", ec);
        }
        std::filesystem::rename(mergeTmp.toStdString(), mergeOutput.toStdString(), ec);
        if (ec) {
            return LINGLONG_ERR("rename merge dir", ec);
        }
        newMergedItems.push_back({
          .binaryCommit = binaryCommit,
          .commits = commits,
          .id = mergeID,
          .modules = modules,
          .name = it.first,
        });
    }
    // 保存merged记录
    auto ret = this->cache->updateMergedItems(newMergedItems);
    if (!ret.has_value()) {
        return LINGLONG_ERR("update merged items", ret);
    }
    // 清理merged无效目录
    auto iter = std::filesystem::directory_iterator(mergedDir.path().toStdString(), ec);
    if (ec) {
        return LINGLONG_ERR("read merge directory", ec);
    }
    for (const auto &entry : iter) {
        auto leak = true;
        for (const auto &mergedItem : newMergedItems) {
            if (entry.path().filename() == mergedItem.id) {
                leak = false;
            }
        }
        if (leak) {
            std::filesystem::remove_all(entry.path(), ec);
            if (ec) {
                qWarning() << ec.message().c_str();
            }
        }
    }
    return LINGLONG_OK;
}

utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>>
OSTreeRepo::listLocalBy(const linglong::repo::repoCacheQuery &query) const noexcept
{
    return this->cache->queryLayerItem(query);
}

QString getOriginRawExec(const QString &execArgs, [[maybe_unused]] const QString &id)
{
    // Note: These strings have appeared in the app-conf-generator.sh of linglong-builder.
    // We need to remove them.

    const QString oldExec = "--exec ";
    const QString newExec = "-- ";

    auto index = execArgs.indexOf(oldExec);
    if (index != -1) {
        return execArgs.mid(index + oldExec.length());
    }

    index = execArgs.indexOf(newExec);
    if (index != -1) {
        return execArgs.mid(index + newExec.length());
    }

    qCritical() << "'-- ' or '--exec ' is not exist in" << execArgs << ", return an empty string";
    return "";
}

QString buildDesktopExec(QString origin, const QString &appID) noexcept
{
    auto newExec = QString{ "%1 run %2 " }.arg(LINGLONG_CLIENT_PATH, appID);

    if (origin.isEmpty()) {
        Q_ASSERT(false);
        return newExec;
    }

    auto *begin = origin.begin();
    while (true) {
        if (begin == origin.end()) {
            break;
        }

        begin = std::find(begin, origin.end(), '%');
        if (begin == origin.end()) {
            break;
        }

        auto *next = begin + 1;
        if (next == origin.end()) {
            break;
        }

        if (*next == '%') {
            begin = next + 1;
            continue;
        }

        QString code{ *next };
        switch (next->toLatin1()) {
        case 'f':
            [[fallthrough]];
        case 'F': {
            origin.insert(next - origin.begin(), '%');
            auto tmp = QString{ "--file %%1 -- -- %2" }.arg(std::move(code), std::move(origin));
            newExec.append(tmp);
            return newExec;
        }
        case 'u':
            [[fallthrough]];
        case 'U': {
            origin.insert(next - origin.begin(), '%');
            auto tmp = QString{ "--url %%1 -- -- %2" }.arg(std::move(code), std::move(origin));
            newExec.append(tmp);
            return newExec;
        }
        default: {
            qDebug() << "no need to mapping" << *next;
        } break;
        }

        break;
    }

    return newExec.append(QString{ "-- %1" }.arg(origin));
}

utils::error::Result<void> desktopFileRewrite(const QString &filePath, const QString &id)
{
    LINGLONG_TRACE("rewrite desktop file " + filePath);

    auto file = utils::GKeyFileWrapper::New(filePath);
    if (!file) {
        return LINGLONG_ERR(file);
    }

    const auto groups = file->getGroups();
    // set Exec
    for (const auto &group : groups) {
        auto hasExecRet = file->hasKey("Exec", group);
        if (!hasExecRet) {
            return LINGLONG_ERR(hasExecRet);
        }
        const auto &hasExec = *hasExecRet;
        if (!hasExec) {
            qWarning() << "No Exec section in" << group << ", set a default value";
            auto defaultExec = QString("%1 run %2").arg(LINGLONG_CLIENT_PATH, id);
            file->setValue("Exec", defaultExec, group);
            continue;
        }

        auto originExec = file->getValue<QString>("Exec", group);
        if (!originExec) {
            return LINGLONG_ERR(originExec);
        }
        const auto &originExecStr = originExec->toStdString();

        auto rawExec = *originExec;
        if (originExec->contains(LINGLONG_CLIENT_NAME)) {
            qDebug() << "The Exec section in" << filePath << "has been generated, rewrite again.";
            rawExec = getOriginRawExec(*originExec, id);
        }

        file->setValue("Exec", buildDesktopExec(rawExec, id), group);
    }

    file->setValue("TryExec", LINGLONG_CLIENT_PATH, utils::GKeyFileWrapper::DesktopEntry);
    file->setValue("X-linglong", id, utils::GKeyFileWrapper::DesktopEntry);

    // save file
    auto ret = file->saveToFile(filePath);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> dbusServiceRewrite(const QString &filePath, const QString &id)
{
    LINGLONG_TRACE("rewrite dbus service file " + filePath);

    auto file = utils::GKeyFileWrapper::New(filePath);
    if (!file) {
        return LINGLONG_ERR(file);
    }

    auto hasExecRet = file->hasKey("Exec", utils::GKeyFileWrapper::DBusService);
    if (!hasExecRet) {
        return LINGLONG_ERR(hasExecRet);
    }
    const auto &hasExec = *hasExecRet;
    if (!hasExec) {
        qWarning() << "DBus service" << filePath << "has no Exec Section.";
        return LINGLONG_OK;
    }

    auto originExec = file->getValue<QString>("Exec", utils::GKeyFileWrapper::DBusService);
    if (!originExec) {
        return LINGLONG_ERR(originExec);
    }

    auto rawExec = *originExec;
    if (originExec->contains(LINGLONG_CLIENT_NAME)) {
        qDebug() << "The Exec section in" << filePath << "has been generated, rewrite again.";
        rawExec = getOriginRawExec(*originExec, id);
    }

    auto newExec = QString("%1 run %2 -- %3").arg(LINGLONG_CLIENT_PATH, id, rawExec);
    file->setValue("Exec", newExec, utils::GKeyFileWrapper::DBusService);

    auto ret = file->saveToFile(filePath);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> systemdServiceRewrite(const QString &filePath, const QString &id)
{
    LINGLONG_TRACE("rewrite systemd user service " + filePath);

    // Related doc: https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html
    // NOTE: The key is allowed to be repeated in the service group
    QStringList execKeys{ "ExecStart", "ExecStartPost", "ExecCondition",
                          "ExecStop",  "ExecStopPost",  "ExecReload" };
    auto file = utils::GKeyFileWrapper::New(filePath);
    if (!file) {
        return LINGLONG_ERR(file);
    }

    auto keys = file->getkeys(utils::GKeyFileWrapper::SystemdService);
    if (!keys) {
        return LINGLONG_ERR(keys);
    }
    for (const auto &key : *keys) {
        if (!execKeys.contains(key)) {
            continue;
        }

        auto originExec = file->getValue<QString>(key, utils::GKeyFileWrapper::SystemdService);
        if (!originExec) {
            return LINGLONG_ERR(originExec);
        }

        auto rawExec = *originExec;
        if (originExec->contains(LINGLONG_CLIENT_NAME)) {
            qDebug() << "The Exec section in" << filePath << "has been generated, rewrite again.";
            rawExec = getOriginRawExec(*originExec, id);
        }

        auto newExec = QString("%1 run %2 -- %3").arg(LINGLONG_CLIENT_PATH, id, rawExec);
        file->setValue(key, newExec, utils::GKeyFileWrapper::SystemdService);
    }

    auto ret = file->saveToFile(filePath);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> contextMenuRewrite(const QString &filePath, const QString &id)
{
    LINGLONG_TRACE("rewrite context menu" + filePath);

    auto file = utils::GKeyFileWrapper::New(filePath);
    if (!file) {
        return LINGLONG_ERR(file);
    }

    auto groups = file->getGroups();
    // set Exec
    for (const auto &group : groups) {
        auto hasExecRet = file->hasKey("Exec", group);
        if (!hasExecRet) {
            return LINGLONG_ERR(hasExecRet);
        }
        const auto &hasExec = *hasExecRet;
        // first group has no Exec, just skip it
        if (!hasExec) {
            continue;
        }

        auto originExec = file->getValue<QString>("Exec", group);
        if (!originExec) {
            return LINGLONG_ERR(originExec);
        }
        auto rawExec = *originExec;
        if (originExec->contains(LINGLONG_CLIENT_NAME)) {
            qDebug() << "The Exec section in" << filePath << "has been generated, rewrite again.";
            rawExec = getOriginRawExec(*originExec, id);
        }

        file->setValue("Exec", buildDesktopExec(rawExec, id), group);
    }

    auto ret = file->saveToFile(filePath);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::IniLikeFileRewrite(const QFileInfo &info,
                                                          const QString &id) noexcept
{
    LINGLONG_TRACE("ini-like file rewrite");

    if (info.path().contains("share/applications") && info.suffix() == "desktop") {
        auto ret = desktopFileRewrite(info.absoluteFilePath(), id);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        // In KDE environment, every desktop should own the executable permission
        // We just set the file permission to 0755 here.
        if (!QFile::setPermissions(info.absoluteFilePath(),
                                   QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                     | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                     | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                     | QFileDevice::ExeOther)) {
            qCritical() << "Failed to chmod" << info.absoluteFilePath();
            Q_ASSERT(false);
        }

    } else if (info.path().contains("share/dbus-1") && info.suffix() == "service") {
        return dbusServiceRewrite(info.absoluteFilePath(), id);
    } else if (info.path().contains("share/systemd/user") && info.suffix() == "service") {
        return systemdServiceRewrite(info.absoluteFilePath(), id);
    } else if (info.path().contains("share/applications/context-menus")
               && info.suffix() == "conf") {
        return contextMenuRewrite(info.absoluteFilePath(), id);
    }

    return LINGLONG_OK;
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace linglong::repo
