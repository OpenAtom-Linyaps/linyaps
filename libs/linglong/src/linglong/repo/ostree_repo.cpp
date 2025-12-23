/*
 * SPDX-FileCopyrightText: 2022-2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "api/ClientAPI.h"
#include "linglong/api/types/v1/ExportDirs.hpp"
#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/api/types/v1/RepositoryCacheMergedItem.hpp"
#include "linglong/common/strings.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/config.h"
#include "linglong/utils/command/cmd.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gkeyfile_wrapper.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"

#include <gio/gio.h>
#include <glib.h>
#include <nlohmann/json.hpp>
#include <ostree-repo.h>

#include <QCryptographicHash>
#include <QDateTime>
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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::repo {

namespace {

struct ostreeUserData
{
    bool caught_error{ false };
    guint fetched{ 0 };
    guint requested{ 0 };
    guint bytes_transferred{ 0 };
    char *ostree_status{ nullptr };
    service::Task *taskContext{ nullptr };
    guint64 needed_archived{ 0 };
    guint64 needed_unpacked{ 0 };
    guint64 needed_objects{ 0 };
    double progress{ 0 };

    ~ostreeUserData() { g_clear_pointer(&ostree_status, g_free); }
};

void progress_changed(OstreeAsyncProgress *progress, gpointer user_data)
{
    auto *data = static_cast<ostreeUserData *>(user_data);
    g_clear_pointer(&data->ostree_status, g_free);

    ostree_async_progress_get(progress,
                              "fetched",
                              "u",
                              &data->fetched,
                              "requested",
                              "u",
                              &data->requested,
                              "caught-error",
                              "b",
                              &data->caught_error,
                              "bytes-transferred",
                              "t",
                              &data->bytes_transferred,
                              "status",
                              "s",
                              &data->ostree_status,
                              nullptr);

    if (data->requested == 0) {
        return;
    }

    guint64 request, fetched;
    // use needed_archived to calculate progress if possible
    // otherwise use requested and fetched
    if (data->needed_archived) {
        request = data->needed_archived;
        fetched = data->bytes_transferred;
    } else {
        request = data->requested;
        fetched = data->fetched;
    }

    // report actual fetched and requestd data
    data->taskContext->reportDataHandled(data->fetched, data->requested);

    data->progress =
      std::min(std::max(static_cast<double>(fetched) / request * 100.0, data->progress), 100.0);
    data->taskContext->updateProgress(data->progress);
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

    auto spec = ref.channel + "/" + ref.id + "/" + ref.version.toString() + "/"
      + ref.arch.toStdString() + "/" + module;

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
    auto ret = ref.channel + "/" + ref.id + "/" + ref.version.toString() + "/"
      + ref.arch.toStdString() + "/" + module;

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

utils::error::Result<void>
updateOstreeRepoConfig(OstreeRepo *repo,
                       const linglong::api::types::v1::RepoConfigV2 &config,
                       const QString &parent = "") noexcept
{
    LINGLONG_TRACE("update configuration");

    g_autoptr(GError) gErr = nullptr;

    g_auto(GStrv) remoteNames = ostree_repo_remote_list(repo, nullptr);

    // 删除全部仓库
    if (remoteNames != nullptr) {
        for (auto remoteName = remoteNames; *remoteName != nullptr; remoteName++) {
            if (ostree_repo_remote_change(repo,
                                          nullptr,
                                          OSTREE_REPO_REMOTE_CHANGE_DELETE,
                                          *remoteName,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          &gErr)
                == FALSE) {
                return LINGLONG_ERR("ostree_repo_remote_change", gErr);
            }
        }
    }

    for (const auto &repoCfg : config.repos) {
        const std::string &remoteUrl = repoCfg.url + "/repos/" + repoCfg.name;
        g_autoptr(GVariant) options = NULL;
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&builder, "{sv}", "gpg-verify", g_variant_new_boolean(FALSE));
        // NOTE:
        // libcurl 8.2.1 has a http2 bug https://github.com/curl/curl/issues/11859
        // We disable http2 for now.
        g_variant_builder_add(&builder, "{sv}", "http2", g_variant_new_boolean(FALSE));
        // add contenturl to use mirrorlist
        if (repoCfg.mirrorEnabled.value_or(false)) {
            auto mirrorlist = std::string("mirrorlist=") + repoCfg.url + "/api/v2/mirrors/stable";
            g_variant_builder_add(&builder,
                                  "{sv}",
                                  "contenturl",
                                  g_variant_new_string(mirrorlist.c_str()));
        }
        options = g_variant_ref_sink(g_variant_builder_end(&builder));

        if (ostree_repo_remote_change(repo,
                                      nullptr,
                                      OSTREE_REPO_REMOTE_CHANGE_ADD,
                                      repoCfg.alias.value_or(repoCfg.name).c_str(),
                                      remoteUrl.c_str(),
                                      options,
                                      nullptr,
                                      &gErr)
            == FALSE) {
            return LINGLONG_ERR("ostree_repo_remote_change", gErr);
        }
    }

    GKeyFile *configKeyFile = ostree_repo_get_config(repo);
    Q_ASSERT(configKeyFile != nullptr);

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

utils::error::Result<OstreeRepo *>
createOstreeRepo(const QDir &location,
                 const linglong::api::types::v1::RepoConfigV2 &config,
                 const QString &parent = "") noexcept
{
    LINGLONG_TRACE(
      fmt::format("create linglong repository at {}", location.absolutePath().toStdString()));

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

    auto result = updateOstreeRepoConfig(ostreeRepo, config, parent);

    if (!result) {
        return LINGLONG_ERR(result);
    }

    return static_cast<OstreeRepo *>(g_steal_pointer(&ostreeRepo));
}

utils::error::Result<package::Reference> clearReferenceLocal(const linglong::repo::RepoCache &cache,
                                                             package::FuzzyReference fuzzy,
                                                             bool semanticMatching = false) noexcept
{
    LINGLONG_TRACE("clear fuzzy reference locally");

    // the arch of all local packages is host arch
    if (fuzzy.arch) {
        auto curArch = linglong::package::Architecture::currentCPUArchitecture();
        if (!curArch) {
            return LINGLONG_ERR(curArch.error().message(), utils::error::ErrorCode::Unknown);
        }
        if (curArch != *fuzzy.arch) {
            return LINGLONG_ERR("arch mismatch with host arch", utils::error::ErrorCode::Unknown);
        }
    }

    // NOTE: ignore channel, two packages with the same version but different channels are not
    // allowed to be installed
    repoCacheQuery query;
    query.id = fuzzy.id;
    const auto availablePackage = cache.queryLayerItem(query);
    if (availablePackage.empty()) {
        return LINGLONG_ERR("package not found:" + fuzzy.toString(),
                            utils::error::ErrorCode::AppNotFoundFromLocal);
    }

    std::optional<linglong::package::Version> version;
    if (fuzzy.version && !fuzzy.version->empty()) {
        auto ret = linglong::package::Version::parse(fuzzy.version.value());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        version = *ret;
    }

    utils::error::Result<linglong::api::types::v1::RepositoryCacheLayersItem> foundRef =
      LINGLONG_ERR("compatible layer not found", utils::error::ErrorCode::LayerCompatibilityError);
    for (const auto &ref : availablePackage) {
        // we should ignore deleted layers
        if (ref.deleted && ref.deleted.value()) {
            continue;
        }

        auto pkgVer = linglong::package::Version::parse(ref.info.version);
        if (!pkgVer) {
            qFatal("internal error: broken data of repo cache: %s", ref.info.version.c_str());
        }

        LogD("available layer {} found: {}", fuzzy.toString(), ref.info.version);
        if (version) {
            if (semanticMatching && pkgVer->semanticMatch(version->toString())) {
                foundRef = ref;
                break;
            }
            if (*pkgVer == version.value()) {
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

    return package::Reference::fromPackageInfo(foundRef->info);
};

utils::error::Result<bool> semanticMatch(const package::FuzzyReference &fuzzy,
                                         const api::types::v1::PackageInfoV2 &record) noexcept
{
    LINGLONG_TRACE("semanticMatch");

    if (fuzzy.id != record.id) {
        return false;
    }

    if (fuzzy.channel && fuzzy.channel != record.channel) {
        return false;
    }

    if (fuzzy.arch && fuzzy.arch->toStdString() != record.arch[0]) {
        return false;
    }

    if (fuzzy.version) {
        auto version = package::Version::parse(record.version);
        if (!version) {
            return LINGLONG_ERR(version);
        }
        if (!version->semanticMatch(*fuzzy.version)) {
            return false;
        }
    }

    return true;
}

std::optional<package::Reference> matchReference(const api::types::v1::PackageInfoV2 &record,
                                                 const package::FuzzyReference &fuzzy,
                                                 const std::string &module) noexcept
{
    auto recordStr = nlohmann::json(record).dump();
    if (fuzzy.channel && fuzzy.channel != record.channel) {
        return std::nullopt;
    }
    if (fuzzy.id != record.id) {
        return std::nullopt;
    }
    auto version = package::Version::parse(record.version);
    if (!version) {
        qWarning() << "Ignore invalid package record" << recordStr.c_str() << version.error();
        return std::nullopt;
    }
    if (record.arch.empty()) {
        qWarning() << "Ignore invalid package record";
        return std::nullopt;
    }
    if (module == "binary") {
        if (record.packageInfoV2Module != "binary" && record.packageInfoV2Module != "runtime") {
            return std::nullopt;
        }
    } else {
        if (record.packageInfoV2Module != module) {
            return std::nullopt;
        }
    }
    auto arch = package::Architecture::parse(record.arch[0]);
    if (!arch) {
        qWarning() << "Ignore invalid package record" << recordStr.c_str() << arch.error();
        return std::nullopt;
    }

    auto currentRef = package::Reference::create(record.channel, fuzzy.id, *version, *arch);
    if (!currentRef) {
        qWarning() << "Ignore invalid package record" << recordStr.c_str() << currentRef.error();
        return std::nullopt;
    }
    return *currentRef;
}

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
                                    &gErr)) {
        if (layer.commit != rev) {
            LogD("skip unset ref on mismatched commit");
            return LINGLONG_OK;
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
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::handleRepositoryUpdate(
  QDir layerDir, const api::types::v1::RepositoryCacheLayersItem &layer) noexcept
{
    std::string refspec = ostreeRefSpecFromLayerItem(layer);
    LINGLONG_TRACE(fmt::format("checkout {} from ostree repository to layers dir", refspec));

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
    LINGLONG_TRACE(fmt::format("ensure empty layer dir exist {}", commit));

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
        if (ec) {
            return LINGLONG_ERR(
              QString{ "failed to create layer dir %1: %2" }.arg(dir.c_str(),
                                                                 ec.message().c_str()));
        }
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

OSTreeRepo::OSTreeRepo(const QDir &path, api::types::v1::RepoConfigV2 cfg) noexcept
    : cfg(std::move(cfg))
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

    // To avoid glib start thread
    // set GIO_USE_VFS to local and GVFS_REMOTE_VOLUME_MONITOR_IGNORE to 1
    linglong::utils::command::EnvironmentVariableGuard gioGuard{ "GIO_USE_VFS", "local" };
    linglong::utils::command::EnvironmentVariableGuard gvfsGuard{
        "GVFS_REMOTE_VOLUME_MONITOR_IGNORE",
        "1"
    };

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) repoPath = nullptr;
    g_autoptr(OstreeRepo) ostreeRepo = nullptr;

    this->repoDir = path;

    {
        LINGLONG_TRACE(fmt::format("use linglong repo at {}", path.absolutePath().toStdString()));

        repoPath = g_file_new_for_path(this->ostreeRepoDir().absolutePath().toLocal8Bit());
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

        g_clear_error(&gErr);
        g_clear_object(&ostreeRepo);
    }

    LINGLONG_TRACE("init ostree-based linglong repository");

    auto result = createOstreeRepo(this->ostreeRepoDir().absolutePath(), this->cfg);
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

api::types::v1::RepoConfigV2 OSTreeRepo::getOrderedConfig() const noexcept
{
    auto orderCfg = this->cfg;
    orderCfg.repos = repo::getPrioritySortedRepos(this->cfg);
    return orderCfg;
}

std::vector<std::vector<api::types::v1::Repo>> OSTreeRepo::getPriorityGroupedRepos() const noexcept
{
    return repo::getPriorityGroupedRepos(this->cfg);
}

/*
 * @brief Get the repo by repo alias.
 * @param alias the alias of the repo
 * @return repo
 */
utils::error::Result<api::types::v1::Repo>
OSTreeRepo::getRepoByAlias(const std::string &alias) const noexcept
{
    LINGLONG_TRACE("get repo by alias")
    auto repo = this->cfg.repos;
    if (repo.empty()) {
        return LINGLONG_ERR("no repo found");
    }
    auto it = std::find_if(repo.begin(), repo.end(), [&alias](const api::types::v1::Repo &repo) {
        return repo.alias.value_or(repo.name) == alias;
    });
    if (it == repo.end()) {
        return LINGLONG_ERR("no repo found");
    }
    return *it;
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
    result = updateOstreeRepoConfig(this->ostreeRepo.get(), newCfg);
    transaction.addRollBack([this]() noexcept {
        auto result = updateOstreeRepoConfig(this->ostreeRepo.get(), this->cfg);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();

    this->cfg = newCfg;

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::setConfig(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    LINGLONG_TRACE("set config");
    LogD("set config: {}", nlohmann::json(cfg).dump());

    utils::Transaction transaction;

    LogI("save config to disk");
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
    LogI("update ostree repo config");
    result = updateOstreeRepoConfig(this->ostreeRepo.get(), cfg);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    transaction.addRollBack([this]() noexcept {
        auto result = updateOstreeRepoConfig(this->ostreeRepo.get(), this->cfg);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    LogI("rebuild repo cache");
    if (auto ret = this->cache->rebuildCache(cfg, *(this->ostreeRepo)); !ret) {
        return LINGLONG_ERR(ret);
    }

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

    auto info = dir.info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    auto reference = package::Reference::fromPackageInfo(*info);
    if (!reference) {
        return LINGLONG_ERR(reference);
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

    return package::LayerDir{ layerDir->absolutePath() };
}

[[nodiscard]] utils::error::Result<void> OSTreeRepo::push(const package::Reference &reference,
                                                          const std::string &module) const noexcept
{
    const auto &defaultRepo = getDefaultRepo();
    return pushToRemote(defaultRepo.name, defaultRepo.url, reference, module);
}

utils::error::Result<void> OSTreeRepo::pushToRemote(const std::string &remoteRepo,
                                                    const std::string &url,
                                                    const package::Reference &reference,
                                                    const std::string &module) const noexcept
{
    LINGLONG_TRACE(fmt::format("push {}", reference.toString()));
    auto layerDir = this->getLayerDir(reference, module);
    if (!layerDir) {
        return LINGLONG_ERR("layer not found", layerDir);
    }
    auto env = QProcessEnvironment::systemEnvironment();
    auto client = this->createClientV2(url);

    // 登录认证
    auto envUsername = env.value("LINGLONG_USERNAME").toUtf8();
    auto envPassword = env.value("LINGLONG_PASSWORD").toUtf8();
    request_auth_t auth;
    auth.username = envUsername.data();
    auth.password = envPassword.data();
    auto signRes = client->signIn(&auth);
    if (!signRes) {
        return LINGLONG_ERR("sign error");
    }
    if (signRes->code != 200) {
        const auto *msg = signRes->msg ? signRes->msg : "cannot send request to remote server";
        return LINGLONG_ERR(QString("sign error(%1): %2").arg(auth.username, msg));
    }
    auto *token = signRes->data->token;
    // 创建上传任务
    schema_new_upload_task_req_t newTaskReq;
    auto refStr = ostreeSpecFromReferenceV2(reference, std::nullopt, module);
    newTaskReq.ref = const_cast<char *>(refStr.c_str());
    newTaskReq.repo_name = const_cast<char *>(remoteRepo.c_str());
    auto newTaskRes = client->newUploadTaskID(token, &newTaskReq);
    if (!newTaskRes) {
        return LINGLONG_ERR("create task error");
    }
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

    const auto tarFileName = fmt::format("{}.tgz", reference.id);
    const QString tarFilePath =
      QDir::cleanPath(tmpDir.filePath(QString::fromStdString(tarFileName)));
    QStringList args = { "-zcf", tarFilePath, "-C", layerDir->absolutePath(), "." };
    auto tarStdout = utils::command::Cmd("tar").exec(args);
    if (!tarStdout) {
        return LINGLONG_ERR(tarStdout);
    }

    // 上传文件, 原来的binary_t需要将文件存储到内存，对大文件上传不友好，改为存储文件名
    // 底层改用 curl_mime_filedata 替换 curl_mime_data
    auto filepath = tarFilePath.toUtf8();
    binary_t binary;
    binary.filepath = filepath.data();
    binary.filename = const_cast<char *>(tarFileName.data());
    auto uploadTaskRes = client->uploadTaskFile(token, taskID, &binary);
    if (!uploadTaskRes) {
        return LINGLONG_ERR(QString("upload file error(%1)").arg(taskID));
    }
    if (uploadTaskRes->code != 200) {
        const auto *msg =
          uploadTaskRes->msg ? uploadTaskRes->msg : "cannot send request to remote server";
        return LINGLONG_ERR(QString("upload file error(%1): %2").arg(taskID, msg));
    }
    // 查询任务状态
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto uploadInfo = client->uploadTaskInfo(token, taskID);
        if (!uploadInfo) {
            return LINGLONG_ERR(QString("get upload info error(%1)").arg(taskID));
        }
        if (uploadInfo->code != 200) {
            auto msg = uploadInfo->msg ? uploadInfo->msg : "cannot send request to remote server";
            return LINGLONG_ERR(QString("get upload info error(%1): %2").arg(taskID, msg));
        }

        LogI("pushing {}/{} status: {}", reference.toString(), module, uploadInfo->data->status);
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
    LINGLONG_TRACE(fmt::format("remove {}", ref.toString()));

    if (module.empty()) {
        return LINGLONG_ERR("module is empty");
    }

    auto layer = this->getLayerItem(ref, module, subRef);
    if (!layer) {
        return LINGLONG_ERR(layer);
    }

    return remove(*layer);
}

utils::error::Result<void>
OSTreeRepo::remove(const api::types::v1::RepositoryCacheLayersItem &item) noexcept
{
    LINGLONG_TRACE(fmt::format("remove commit {}", item.commit));

    // unset ostree ref
    auto res = this->removeOstreeRef(item);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    // delete from db
    res = this->cache->deleteLayerItem(item);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    // remove deployed layer
    auto removed = undeployedLayer(item);
    if (!removed) {
        // continue anyway since we've already unset the ostree ref and deleted from db
        LogW("failed to remove layer dir: {}", removed.error());
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
OSTreeRepo::undeployedLayer(const api::types::v1::RepositoryCacheLayersItem &layer) noexcept
{
    LINGLONG_TRACE(fmt::format("undeployed layer {}", layer.commit));

    // It is crucial to remove the layer directory by commit
    auto layerDir = getLayerDir(layer);
    if (!layerDir) {
        LogW("layer dir not found, skip remove: {}", layerDir.error());
        return LINGLONG_OK;
    }

    if (!layerDir->removeRecursively()) {
        return LINGLONG_ERR(fmt::format("failed to remove layer dir {}", layerDir->absolutePath()));
    }

    // In older versions, LINGLONG_ROOT/layers/<commit> could be a symlink pointing to
    // LINGLONG_ROOT/layers/<appid>/<version>/<arch>.
    // This code attempts to clean up any empty parent directories up to `LINGLONG_ROOT/layers/
    // for compatibility,
    QFileInfo dirInfo{ layerDir->absolutePath() };
    if (!dirInfo.isSymLink()) {
        return LINGLONG_OK;
    }

    QDir target = dirInfo.symLinkTarget();
    QDir topLevel = dirInfo.absoluteDir().absolutePath();
    target.cdUp();
    while (topLevel.relativeFilePath(target.absolutePath()) != ".") {
        if (target.isEmpty() && !QFile::remove(target.absolutePath())) {
            LogW("failed to remove {}", target.absolutePath());
        }

        if (!target.cdUp()) {
            LogW("failed to cd up from {}", target.absolutePath());
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

// 初始化一个GVariantBuilder
GVariantBuilder OSTreeRepo::initOStreePullOptions(const std::string &ref) noexcept
{
    std::array<const char *, 2> refs{ ref.c_str(), nullptr };
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    std::string userAgent = "linglong/" LINGLONG_VERSION;
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "append-user-agent",
                          g_variant_new_variant(g_variant_new_string(userAgent.c_str())));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-static-deltas",
                          g_variant_new_variant(g_variant_new_boolean(true)));

    g_variant_builder_add(&builder,
                          "{s@v}",
                          "refs",
                          g_variant_new_variant(g_variant_new_strv(refs.data(), -1)));
    return builder;
}

// 在pull之前获取commit size，用于计算进度，需要服务器支持ostree.sizes
utils::error::Result<std::vector<guint64>>
OSTreeRepo::getCommitSize(const std::string &remote, const std::string &refString) noexcept
{
    LINGLONG_TRACE("get commit size " + refString);
#if OSTREE_CHECK_VERSION(2020, 1)
    g_autoptr(GError) gErr = nullptr;
    GVariantBuilder builder = this->initOStreePullOptions(refString);
    // 设置flags只获取commit的metadata
    g_variant_builder_add(
      &builder,
      "{s@v}",
      "flags",
      g_variant_new_variant(g_variant_new_int32(OSTREE_REPO_PULL_FLAGS_COMMIT_ONLY)));
    g_autoptr(GVariant) pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));
    // 获取commit的metadata
    auto status = ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                                remote.c_str(),
                                                pull_options,
                                                nullptr,
                                                nullptr,
                                                &gErr);
    if (status == FALSE) {
        return LINGLONG_ERR("ostree_repo_pull", gErr);
    }
    // 使用refString获取commit的sha256
    g_autofree char *resolved_rev = NULL;
    if (!ostree_repo_resolve_rev(this->ostreeRepo.get(),
                                 refString.c_str(),
                                 FALSE,
                                 &resolved_rev,
                                 &gErr)) {
        return LINGLONG_ERR("ostree_repo_resolve_rev", gErr);
    }
    // 使用sha256获取commit id
    g_autoptr(GVariant) commit = NULL;
    if (!ostree_repo_load_variant(this->ostreeRepo.get(),
                                  OSTREE_OBJECT_TYPE_COMMIT,
                                  resolved_rev,
                                  &commit,
                                  &gErr)) {
        return LINGLONG_ERR("ostree_repo_load_variant", gErr);
    }
    g_autoptr(GPtrArray) sizes = NULL;
    // 获取commit中的所有对象大小
    if (!ostree_commit_get_object_sizes(commit, &sizes, &gErr))
        return LINGLONG_ERR("ostree_commit_get_object_sizes", gErr);
    // 计算需要下载的文件大小
    guint64 needed_archived = 0;
    // 计算需要解压的文件大小
    guint64 needed_unpacked = 0;
    // 计算需要下载的文件数量
    guint64 needed_objects = 0;
    // 遍历commit中的所有对象，如果对象不存在，则需要下载
    for (guint i = 0; i < sizes->len; i++) {
        OstreeCommitSizesEntry *entry = (OstreeCommitSizesEntry *)sizes->pdata[i];
        gboolean exists;
        if (!ostree_repo_has_object(this->ostreeRepo.get(),
                                    entry->objtype,
                                    entry->checksum,
                                    &exists,
                                    NULL,
                                    &gErr))
            return LINGLONG_ERR("ostree_repo_has_object", gErr);

        // Object not in local repo, so we need to download it
        if (!exists) {
            needed_archived += entry->archived;
            needed_unpacked += entry->unpacked;
            needed_objects++;
        }
    }
    // 删除ref
    if (!ostree_repo_set_ref_immediate(this->ostreeRepo.get(),
                                       remote.c_str(),
                                       refString.c_str(),
                                       nullptr,
                                       nullptr,
                                       &gErr)) {
        return LINGLONG_ERR("ostree_repo_set_ref_immediate", gErr);
    }
    return std::vector<guint64>{ needed_archived, needed_unpacked, needed_objects };
#else
    return LINGLONG_ERR("ostree_repo_pull_with_options is not supported");
#endif
}

utils::error::Result<void> OSTreeRepo::pull(service::Task &taskContext,
                                            const package::Reference &reference,
                                            const std::string &module,
                                            const api::types::v1::Repo &repo) noexcept
{
    // Note: if module is runtime, refString will be channel:id/version/binary.
    // because we need considering update channel:id/version/runtime to channel:id/version/binary.
    auto refString = ostreeSpecFromReferenceV2(reference, std::nullopt, module);
    auto repoName = repo.alias.value_or(repo.name);
    LINGLONG_TRACE(fmt::format("pull {} from {}", refString, repoName));

    auto *cancellable = taskContext.cancellable();

    ostreeUserData data{ .taskContext = &taskContext };

    auto sizes = this->getCommitSize(repoName, refString);
    if (!sizes.has_value()) {
        LogD("get commit size error: {}", sizes.error().message());
    } else if (sizes->size() >= 3) {
        data.needed_archived = sizes->at(0);
        data.needed_unpacked = sizes->at(1);
        data.needed_objects = sizes->at(2);
    }

    g_autoptr(OstreeAsyncProgress) progress =
      ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
    Q_ASSERT(progress != nullptr);

    g_autoptr(GError) gErr = nullptr;

    auto builder = this->initOStreePullOptions(refString);
    g_autoptr(GVariant) pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));
    // 这里不能使用g_main_context_push_thread_default，因为会阻塞Qt的事件循环

    auto status = ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                                repoName.c_str(),
                                                pull_options,
                                                progress,
                                                cancellable,
                                                &gErr);
    ostree_async_progress_finish(progress);
    auto shouldFallback = false;
    if (status == FALSE) {
        // gErr->code is 0, so we compare string here.
        if (!strstr(gErr->message, "No such branch")) {
            return LINGLONG_ERR("ostree_repo_pull_with_options", gErr);
        }
        LogW("ostree_repo_pull_with_options failed with [{}]: {}", gErr->code, gErr->message);
        shouldFallback = true;
    }
    // Note: this fallback is only for binary to runtime
    if (shouldFallback && (module == "binary" || module == "runtime")) {
        g_autoptr(OstreeAsyncProgress) progress =
          ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
        Q_ASSERT(progress != nullptr);
        // fallback to old ref
        refString = ostreeSpecFromReference(reference, std::nullopt, module);
        LogW("fallback to module runtime, pull {}", refString);

        g_clear_error(&gErr);

        GVariantBuilder builder = this->initOStreePullOptions(refString);

        g_autoptr(GVariant) pull_options = g_variant_ref_sink(g_variant_builder_end(&builder));

        status = ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                               repoName.c_str(),
                                               pull_options,
                                               progress,
                                               cancellable,
                                               &gErr);
        ostree_async_progress_finish(progress);
        if (status == FALSE) {
            return LINGLONG_ERR("ostree_repo_pull_with_options", gErr);
        }
    }

    g_autofree char *commit = nullptr;
    g_autoptr(GFile) layerRootDir = nullptr;
    api::types::v1::RepositoryCacheLayersItem item;

    g_clear_error(&gErr);
    if (ostree_repo_read_commit(this->ostreeRepo.get(),
                                refString.c_str(),
                                &layerRootDir,
                                &commit,
                                cancellable,
                                &gErr)
        == 0) {
        return LINGLONG_ERR("ostree_repo_read_commit", gErr);
    }

    g_autoptr(GFile) infoFile = g_file_resolve_relative_path(layerRootDir, "info.json");
    auto info = utils::parsePackageInfo(infoFile);
    if (!info) {
        return LINGLONG_ERR(info);
    }

    item.commit = commit;
    item.info = *info;
    item.repo = repoName;

    auto layerDir = this->ensureEmptyLayerDir(item.commit);
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto result = this->handleRepositoryUpdate(*layerDir, item);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<package::Reference>
OSTreeRepo::clearReference(const package::FuzzyReference &fuzzy,
                           const clearReferenceOption &opts,
                           const std::string &module,
                           const std::optional<std::string> &repo) const noexcept
{
    LINGLONG_TRACE(fmt::format("clear fuzzy reference {}", fuzzy.toString()));

    utils::error::Result<package::Reference> reference = LINGLONG_ERR("reference not exists");

    if (!opts.forceRemote) {
        reference = clearReferenceLocal(*cache, fuzzy, opts.semanticMatching);
        if (reference) {
            return reference;
        }

        if (!opts.fallbackToRemote) {
            return LINGLONG_ERR(reference);
        }

        qInfo() << reference.error();
        qInfo() << "fallback to Remote";
    }

    // TODO
    // repo should not optional
    api::types::v1::Repo remoteRepo = getDefaultRepo();

    if (repo) {
        auto repoRet = this->getRepoByAlias(*repo);
        if (!repoRet) {
            return LINGLONG_ERR(repoRet);
        }
        remoteRepo = *repoRet;
    }

    auto listRet = this->searchRemote(fuzzy, remoteRepo);
    if (!listRet.has_value()) {
        return LINGLONG_ERR("get ref list from remote " + listRet.error().message(),
                            listRet.error().code());
    }

    std::optional<package::Version> fuzzyVersion;
    auto list = std::move(listRet).value();
    if (fuzzy.version) {
        list = package::Version::filterByFuzzyVersion(list, *fuzzy.version);

        auto fuzzyVerRet = linglong::package::Version::parse(*fuzzy.version);
        if (!fuzzyVerRet) {
            return LINGLONG_ERR(fuzzyVerRet);
        }
        fuzzyVersion = std::move(fuzzyVerRet).value();
    }

    for (const auto &record : list) {
        auto currentRefRet = matchReference(record, fuzzy, module);
        if (!currentRefRet) {
            continue;
        }

        if (fuzzyVersion) {
            // 语义化匹配最新的版本
            if (opts.semanticMatching) {
                if (!reference || currentRefRet->version > reference->version) {
                    reference = *currentRefRet;
                }
                continue;
            }

            // 精确匹配版本
            if (currentRefRet->version == *fuzzyVersion) {
                reference = *currentRefRet;
                break;
            }
            continue;
        }

        // 匹配最新版本
        if (!reference || currentRefRet->version > reference->version) {
            reference = *currentRefRet;
        }
    }

    if (!reference) {
        auto msg =
          fmt::format("not found ref:{} module:{} from remote repo", fuzzy.toString(), module);
        return LINGLONG_ERR(msg, utils::error::ErrorCode::AppNotFoundFromRemote);
    }

    return reference;
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listLocal() const noexcept
{
    std::vector<api::types::v1::PackageInfoV2> pkgInfos;
    auto items = this->cache->queryExistingLayerItem();
    for (const auto &item : items) {
        if (item.deleted && item.deleted.value()) {
            continue;
        }

        pkgInfos.emplace_back(item.info);
    }

    return pkgInfos;
}

utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>>
OSTreeRepo::listLayerItem() const noexcept
{
    return this->cache->queryExistingLayerItem();
}

utils::error::Result<int64_t>
OSTreeRepo::getLayerCreateTime(const api::types::v1::RepositoryCacheLayersItem &item) const noexcept
{
    LINGLONG_TRACE("get install time");
    auto dir = this->getLayerDir(item);
    if (!dir.has_value()) {
        return LINGLONG_ERR("get layer dir", dir);
    }
    return QFileInfo(dir->absolutePath()).birthTime().toSecsSinceEpoch();
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listLocalApps() const noexcept
{
    LINGLONG_TRACE("list local latest package");

    auto allPkgs = this->listLocal();
    if (!allPkgs) {
        return LINGLONG_ERR(allPkgs);
    }

    std::vector<api::types::v1::PackageInfoV2> appPkgs;
    for (const auto &pkg : *allPkgs) {
        if (pkg.kind == "app") {
            appPkgs.push_back(pkg);
        }
    }

    // apps sharing the same ID and channel are considered the same app
    std::sort(appPkgs.begin(), appPkgs.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        if (lhs.channel != rhs.channel) {
            return lhs.channel < rhs.channel;
        }

        auto lhsVer = package::Version::parse(lhs.version);
        auto rhsVer = package::Version::parse(rhs.version);
        if (!lhsVer) {
            return false;
        } else if (!rhsVer) {
            return true;
        }
        return *lhsVer > *rhsVer;
    });
    // return only latest version of each app
    appPkgs.erase(std::unique(appPkgs.begin(),
                              appPkgs.end(),
                              [](const auto &lhs, const auto &rhs) {
                                  return lhs.id == rhs.id && lhs.channel == rhs.channel;
                              }),
                  appPkgs.end());

    return appPkgs;
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::searchRemote(std::string searching, const api::types::v1::Repo &repo) const noexcept
{
    LINGLONG_TRACE("search remote packages");

    auto fuzzyRef = package::FuzzyReference::create(std::nullopt,
                                                    std::move(searching),
                                                    std::nullopt,
                                                    std::nullopt);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef);
    }

    return searchRemote(*fuzzyRef, repo);
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::searchRemote(const package::FuzzyReference &fuzzyRef,
                         const api::types::v1::Repo &repo,
                         bool semanticMatching) const noexcept
{
    LINGLONG_TRACE("list remote packages");

    LogD("searchRemote {} [{}] from repo {}",
         fuzzyRef.toString(),
         semanticMatching,
         nlohmann::json(repo).dump());

    auto client = this->createClientV2(repo.url);

    char *app_id = strndup(fuzzyRef.id.data(), fuzzyRef.id.size());
    if (app_id == nullptr) {
        return LINGLONG_ERR(fmt::format("strndup app_id failed: {}", fuzzyRef.id));
    }
    char *repo_name = strndup(repo.name.data(), repo.name.size());
    if (repo_name == nullptr) {
        return LINGLONG_ERR(fmt::format("strndup repo_name failed: {}", repo.name));
    }

    char *channel = nullptr;
    if (fuzzyRef.channel) {
        channel = strndup(fuzzyRef.channel->data(), fuzzyRef.channel->size());
        if (channel == nullptr) {
            return LINGLONG_ERR(fmt::format("strndup channel failed: {}", *fuzzyRef.channel));
        }
    }

    // use prefix matching on version strings when searching the remote server
    char *version = nullptr;
    if (fuzzyRef.version) {
        version = strndup(fuzzyRef.version->data(), fuzzyRef.version->size());
        if (version == nullptr) {
            return LINGLONG_ERR(fmt::format("strndup version failed: {}", *fuzzyRef.version));
        }
    }

    auto defaultArch = package::Architecture::currentCPUArchitecture();
    if (!defaultArch) {
        return LINGLONG_ERR(defaultArch);
    }

    auto arch = fuzzyRef.arch.value_or(*defaultArch).toStdString();
    char *archStr = strndup(arch.data(), arch.size());
    if (archStr == nullptr) {
        return LINGLONG_ERR(fmt::format("strndup arch failed: {}", arch));
    }

    auto req = request_fuzzy_search_req_create(app_id, archStr, channel, repo_name, version);
    if (!req) {
        return LINGLONG_ERR("failed to create request");
    }
    auto freeIfNotNull = utils::finally::finally([req] {
        request_fuzzy_search_req_free(req);
    });
    auto response = client->fuzzySearch(req);
    if (!response) {
        return LINGLONG_ERR("failed to send request to remote server\nIf the network is slow, "
                            "set a longer timeout via the LINGLONG_CONNECT_TIMEOUT environment "
                            "variable (current default: 5 seconds).",
                            utils::error::ErrorCode::NetworkError);
    }

    if (response->code != 200) {
        QString msg = (response->msg != nullptr)
          ? response->msg
          : QString{ "cannot send request to remote server: %1\nIf the network is slow, "
                     "set a longer timeout via the LINGLONG_CONNECT_TIMEOUT environment "
                     "variable (current default: 5 seconds)." }
              .arg(response->code);

        return LINGLONG_ERR(msg,
                            (response->msg != nullptr ? utils::error::ErrorCode::Failed
                                                      : utils::error::ErrorCode::NetworkError));
    }

    if (response->data == nullptr || response->data->count == 0) {
        return {};
    }

    std::vector<api::types::v1::PackageInfoV2> pkgInfos;
    pkgInfos.reserve(response->data->count);
    for (auto *entry = response->data->firstEntry; entry != nullptr; entry = entry->nextListEntry) {
        auto *item = (request_register_struct_t *)entry->data;
        auto packageInfo = api::types::v1::PackageInfoV2{
            .arch = { item->arch },
            .base = { item->base },
            .channel = item->channel,
            .description = item->description,
            .id = item->app_id,
            .kind = item->kind,
            .packageInfoV2Module = item->module,
            .name = item->name,
            .runtime = item->runtime,
            .size = item->size,
            .version = item->version,
        };

        // apply semantic matching to search results to correctly filter:
        // versions like app/1.10 when match for app/1.1
        // id like app.1 when match for app
        if (semanticMatching) {
            auto matched = semanticMatch(fuzzyRef, packageInfo);
            if (!matched) {
                LogE("invalid packageInfo", matched.error());
                continue;
            }

            if (!*matched) {
                continue;
            }
        }

        pkgInfos.emplace_back(std::move(packageInfo));
    }

    return std::move(pkgInfos);
}

utils::error::Result<repo::RemotePackages>
OSTreeRepo::matchRemoteByPriority(const package::FuzzyReference &fuzzyRef,
                                  const std::optional<api::types::v1::Repo> &repo) const noexcept
{
    LINGLONG_TRACE("match remote packages by priority");

    repo::RemotePackages remotePackages;

    if (repo) {
        auto list = this->searchRemote(fuzzyRef, *repo, true);
        if (!list) {
            return LINGLONG_ERR(fmt::format("failed to search remote packages from {}", repo->name),
                                utils::error::ErrorCode::NetworkError);
        }

        if (!list->empty()) {
            remotePackages.addPackages(*repo, std::move(list).value());
        }
    } else {
        bool allError = true;
        auto repos = this->getPriorityGroupedRepos();
        for (const auto &repoGroup : repos) {
            for (const auto &repo : repoGroup) {
                auto list = this->searchRemote(fuzzyRef, repo, true);
                if (!list) {
                    LogW("failed to search remote packages from {}: {}", repo.name, list.error());
                    continue;
                }
                allError = false;

                if (list->empty()) {
                    continue;
                }

                remotePackages.addPackages(repo, std::move(list).value());
            }

            // try a lower-priority repo when no matched result
            if (!remotePackages.empty()) {
                break;
            }
        }

        if (allError) {
            return LINGLONG_ERR("failed to search remote packages",
                                utils::error::ErrorCode::NetworkError);
        }
    }

    return remotePackages;
}

void OSTreeRepo::unexportReference(const std::string &layerDir) noexcept
{
    QString layerDirStr = layerDir.c_str();
    QDir entriesDir = this->getEntriesDir();
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
            // update-desktop-database and update-mime-database will generate system files in
            // specify a directory. these files do not belong to the applications and should not be
            // printed in the log.
            if (info.absoluteFilePath().contains("share/mime")
                || info.absoluteFilePath().contains("share/applications")) {
                continue;
            }

            // NOTE: Everything in entries should be directory or symbol link.
            // But it can be some cache file, we should not remove it too.
            qWarning() << "Invalid file detected." << info.absoluteFilePath();
            qWarning() << "If the file is a cache or something like that, ignore this warning.";
            continue;
        }

        if (!info.symLinkTarget().startsWith(layerDirStr)) {
            continue;
        }

        if (!entriesDir.remove(it.filePath())) {
            qCritical() << "Failed to remove" << it.filePath();
            Q_ASSERT(false);
        }
    }

    std::function<void(const QString &path)> removeEmptySubdirectories =
      [&removeEmptySubdirectories](const QString &path) {
          QDir dir(path);

          // 获取目录下的所有子目录
          const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

          // 遍历子目录
          for (const QFileInfo &entry : entries) {
              QString subDirPath = entry.absoluteFilePath();

              // 递归调用，先处理子目录
              removeEmptySubdirectories(subDirPath);

              // 检查子目录是否为空
              QDir subDir(subDirPath);
              if (subDir.isEmpty()) {
                  // 如果子目录为空，则删除它
                  if (!subDir.rmdir(subDirPath)) {
                      qDebug() << "Failed to remove directory:" << subDirPath;
                      continue;
                  }
              }
          }
      };
    removeEmptySubdirectories(entriesDir.absolutePath());
    this->updateSharedInfo();
}

void OSTreeRepo::unexportReference(const package::Reference &ref) noexcept
{
    auto layerDir = this->getLayerDir(ref);
    if (!layerDir) {
        LogE("Failed to unexport reference {} to {}", ref.toString(), layerDir.error().message());
        return;
    }

    this->unexportReference(layerDir->absolutePath().toStdString());
}

void OSTreeRepo::exportReference(const package::Reference &ref) noexcept
{
    auto entriesDir = this->getEntriesDir();
    if (!entriesDir.exists()) {
        entriesDir.mkpath(".");
    }
    auto item = this->getLayerItem(ref);
    if (!item.has_value()) {
        LogE("Failed to export {}: {}", ref.toString(), item.error().message());
        Q_ASSERT(false);
        return;
    }
    auto ret = exportEntries(entriesDir.absolutePath().toStdString(), *item);
    if (!ret.has_value()) {
        LogE("Failed to export {}: {}", ref.toString(), ret.error().message());
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
    LINGLONG_TRACE(fmt::format("export {}", source.string()));
    if (max_depth <= 0) {
        LogW("max depth reached, skipping export for {}", source.c_str());
        return LINGLONG_OK;
    }

    std::error_code ec;
    // 检查源目录是否存在
    if (!std::filesystem::is_directory(source, ec)) {
        auto msg = fmt::format("{} is not a directory", source.string());
        if (ec) {
            return LINGLONG_ERR(std::move(msg), ec);
        }
        return LINGLONG_ERR(std::move(msg));
    }
    auto ret = utils::ensureDirectory(destination);
    if (!ret.has_value()) {
        return LINGLONG_ERR(ret);
    }

    auto iterator = std::filesystem::directory_iterator(source, ec);
    if (ec) {
        return LINGLONG_ERR("list directory: " + source.string(), ec);
    }

    // 遍历源目录中的所有文件和子目录
    for (const auto &entry : iterator) {
        const auto &source_path = entry.path();
        // 跳过无效的软链接
        auto status = std::filesystem::status(source_path, ec);
        if (ec) {
            return LINGLONG_ERR("failed to check file status: " + source_path.string(), ec);
        }

        const auto &target_path = destination / source_path.filename();
        // 如果是文件，创建符号链接
        if (std::filesystem::is_regular_file(status)) {
            // linyaps.original结尾的文件是重写之前的备份文件，不应该被导出
            if (common::strings::ends_with(source_path.string(), ".linyaps.original")) {
                continue;
            }
            // 在导出桌面文件和dbus服务文件时，需要修改其中的Exec和TryExec字段
            // 所以会先给原始文件添加.linyaps.original后缀作为备份，修改后的文件保存为原始文件名
            // 导出时，如果存在linyaps.original文件，则优先使用该原始文件进行重写，避免已经重写过的文件被再次重写
            auto info = QFileInfo(target_path.c_str());
            if ((info.path().contains("share/applications") && info.suffix() == "desktop")
                || (info.path().contains("share/dbus-1") && info.suffix() == "service")
                || (info.path().contains("share/systemd/user") && info.suffix() == "service")
                || (info.path().contains("share/applications/context-menus"))) {
                // We should not modify the files of the checked application directly, but
                // should copy them and then modify.
                auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                auto sourceNewPath = QString{ "%1/%2_%3.%4" }
                                       .arg(QFileInfo(source_path.c_str()).absolutePath(),
                                            info.completeBaseName(),
                                            QString::number(timestamp),
                                            info.suffix())
                                       .toStdString();

                // 如果原始文件不存在，当前source作为原始文件
                auto originPath = source_path.string() + ".linyaps.original";
                if (!std::filesystem::exists(originPath, ec)) {
                    std::filesystem::rename(source_path, originPath, ec);
                    if (ec) {
                        return LINGLONG_ERR(fmt::format("failed to backup file: {}", source_path),
                                            ec);
                    }
                }
                // 复制原始文件到临时文件用于重写
                std::filesystem::copy_file(originPath, sourceNewPath, ec);
                if (ec) {
                    return LINGLONG_ERR(
                      fmt::format("failed to copy file from {} to {}", originPath, sourceNewPath),
                      ec);
                }

                // 目前的重写方式不支持重复重写，因此需要判断是否重写过
                // 旧版本直接在经过复制的 source_path 上进行重写，硬链接数为 1
                // 当前版本通过 rename source_path 创建，硬链接数大于 1
                auto hard_link_count = std::filesystem::hard_link_count(originPath, ec);
                if (ec) {
                    return LINGLONG_ERR("get hard link count", ec);
                }
                if (hard_link_count > 1) {
                    auto ret = IniLikeFileRewrite(QFileInfo(sourceNewPath.c_str()), appID.c_str());
                    if (!ret) {
                        LogW("rewrite file failed: {}", ret.error());
                        continue;
                    }
                }
                std::filesystem::rename(sourceNewPath, source_path, ec);
                if (ec) {
                    return LINGLONG_ERR("rename new path", ec);
                }
            }
            auto oldAppDir = this->getDefaultSharedDir().filePath("applications").toStdString();
            auto newAppDir = this->getOverlayShareDir().filePath("applications").toStdString();
            LogD("oldAppDir: {}, newAppDir: {}, target: {}",
                 oldAppDir,
                 newAppDir,
                 target_path.string());
            // 如果配置了overlay并且是applications中的desktop文件，执行特殊的逻辑
            if (oldAppDir != newAppDir
                && common::strings::starts_with(target_path.string(), oldAppDir)
                && common::strings::ends_with(target_path.string(), ".desktop")) {
                auto desktopExists = false;
                // 如果要导出的desktop已存在，则覆盖导出（无论是在default还是overlay中），避免桌面和任务栏的快捷方式失效
                const std::string appDirs[] = { oldAppDir, newAppDir };
                for (const auto &appDir : appDirs) {
                    // 如果目标文件存在，删除再导出
                    std::filesystem::path linkpath =
                      target_path.string().replace(0, oldAppDir.length(), appDir);
                    auto status = std::filesystem::symlink_status(linkpath, ec);
                    if (!ec) {
                        desktopExists = true;
                        auto target = source_path.lexically_relative(linkpath.parent_path());
                        auto res = utils::relinkFileTo(linkpath, target);
                        if (!res) {
                            LogE("failed to link {} to {}", linkpath.string(), target.string());
                        }
                    }
                }
                // 如果desktop在两个目录都不存在，则优先导出到overlay目录
                if (!desktopExists) {
                    std::filesystem::path linkpath =
                      target_path.string().replace(0, oldAppDir.length(), newAppDir);
                    LogD("create parent directories for {}", linkpath);
                    auto ret = utils::ensureDirectory(linkpath.parent_path());
                    if (!ret.has_value()) {
                        return LINGLONG_ERR("create parent dir", ret);
                    }
                    std::filesystem::create_symlink(
                      source_path.lexically_relative(linkpath.parent_path()),
                      linkpath,
                      ec);
                    if (ec) {
                        return LINGLONG_ERR("create symlink failed: " + linkpath.string(), ec);
                    }
                }
                continue;
            }

            // 在destination创建指向source的符号链接
            // 这里使用相对链接，避免容器环境下的路径问题（例如帮助手册）
            auto linkpath = target_path;
            auto target = source_path.lexically_relative(linkpath.parent_path());
            auto res = utils::relinkFileTo(linkpath, target);
            if (!res) {
                LogE("failed to link {} to {}", linkpath.string(), target.string());
            }
            continue;
        }

        if (std::filesystem::is_directory(status)) {
            auto ret = this->exportDir(appID, source_path, target_path, max_depth - 1);
            if (!ret.has_value()) {
                return ret;
            }
            continue;
        }

        LogW("ignore file {} type {}", source_path.string(), static_cast<int>(status.type()));
    }
    return LINGLONG_OK;
}

utils::error::Result<void>
OSTreeRepo::exportEntries(const std::filesystem::path &rootEntriesDir,
                          const api::types::v1::RepositoryCacheLayersItem &item) noexcept
{
    LINGLONG_TRACE(fmt::format("export {}", item.info.id));
    auto layerDir = getLayerDir(item);
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

    // TODO: The current whitelist logic is not very flexible.
    // The application configuration file can be exported after configuring it in the build
    // configuration file(linglong.yaml).
    const std::filesystem::path exportDirConfigPath = LINGLONG_DATA_DIR "/export-dirs.json";
    if (!std::filesystem::exists(exportDirConfigPath)) {
        return LINGLONG_ERR(
          QString{ "this export config file doesn't exist: %1" }.arg(exportDirConfigPath.c_str()));
    }
    auto exportDirConfig =
      linglong::utils::serialize::LoadJSONFile<linglong::api::types::v1::ExportDirs>(
        exportDirConfigPath);
    if (!exportDirConfig) {
        return LINGLONG_ERR(
          QString{ "failed to load export config file: %1" }.arg(exportDirConfigPath.c_str()));
    }

    // 如果存在lib/systemd目录，优先导出lib/systemd，否则导出share/systemd（为兼容旧应用，新应用应该逐步将配置文件放在lib/systemd目录下）
    exists = std::filesystem::exists(appEntriesDir / "lib/systemd/user", ec);
    if (ec) {
        return LINGLONG_ERR("Failed to check the existence of lib/systemd directory: {}", ec);
    }
    if (exists) {
        exportDirConfig->exportPaths.push_back("lib/systemd/user");
    } else {
        exportDirConfig->exportPaths.push_back("share/systemd/user");
    }

    // 导出应用entries目录下的所有文件到玲珑仓库的entries目录下
    for (const auto &path : exportDirConfig->exportPaths) {
        auto source = appEntriesDir / path;
        auto destination = rootEntriesDir / path;
        // 将 share/systemd 目录下的文件导出到 lib/systemd 目录下
        if (path == "share/systemd/user") {
            destination = rootEntriesDir / "lib/systemd/user";
        }

        if (path == "share/deepin-elf-verify") {
            destination = rootEntriesDir / "share/deepin-elf-verify" / item.commit;
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
    auto defaultApplicationDir = QDir(this->repoDir.absoluteFilePath("entries/share/applications"));
    // 自定义desktop安装路径
    auto desktopExportPath = std::string{ LINGLONG_EXPORT_PATH } + "/applications";
    QDir customApplicationDir =
      QDir(this->repoDir.absoluteFilePath(QString{ "entries/%1" }.arg(desktopExportPath.c_str())));
    auto mimeDataDir = QDir(this->repoDir.absoluteFilePath("entries/share/mime"));
    auto glibSchemasDir = QDir(this->repoDir.absoluteFilePath("entries/share/glib-2.0/schemas"));

    QStringList desktopDirs;
    if (defaultApplicationDir.exists()) {
        desktopDirs << defaultApplicationDir.absolutePath();
    }

    if (defaultApplicationDir != customApplicationDir) {
        if (customApplicationDir.exists()) {
            desktopDirs << customApplicationDir.absolutePath();
        } else {
            qWarning() << "warning: custom application dir " << customApplicationDir.absolutePath()
                       << " does not exist, ignoring";
        }
    }

    // 更新 desktop database
    if (!desktopDirs.empty()) {
        auto ret = utils::command::Cmd("update-desktop-database").exec(desktopDirs);
        if (!ret) {
            qWarning() << "warning: failed to update desktop database in " + desktopDirs.join(" ")
                + ": " + QString::fromStdString(ret.error().message());
        }
    }

    // 更新 mime type database
    if (mimeDataDir.exists()) {
        auto ret = utils::command::Cmd("update-mime-database").exec({ mimeDataDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update mime type database in "
                + mimeDataDir.absolutePath() + ": " + ret.error().message().c_str();
        }
    }

    // 更新 glib-2.0/schemas
    if (glibSchemasDir.exists()) {
        auto ret =
          utils::command::Cmd("glib-compile-schemas").exec({ glibSchemasDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update schemas in " + glibSchemasDir.absolutePath()
                + ": " + QString::fromStdString(ret.error().message());
        }
    }
}

bool OSTreeRepo::isMarkedDeleted(const package::Reference &ref,
                                 const std::string &module) const noexcept
{
    auto item = this->getLayerItem(ref, module);
    if (!item) {
        return false;
    }

    auto it = this->cache->findMatchingItem(*item);
    if (!it) {
        return false;
    }

    return (*it)->deleted.has_value() && *item->deleted;
}

utils::error::Result<void>
OSTreeRepo::markDeleted(const package::Reference &ref,
                        bool deleted,
                        const std::string &module,
                        const std::optional<std::string> &subRef) noexcept
{
    LINGLONG_TRACE(fmt::format("mark {} to deleted", ref.toString()));

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
    LINGLONG_TRACE(fmt::format("get latest layer of {}", ref.toString()));
    if (module == "runtime") {
        module = "binary";
    }

    auto queryItem = [this](const repoCacheQuery &query)
      -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
        LINGLONG_TRACE(fmt::format("query item"));

        auto items = this->cache->queryLayerItem(query);
        auto count = items.size();
        if (count > 0) {
            // relay on the cache to ensure the item is the latest one
            return items.front();
        }
        return LINGLONG_ERR(fmt::format("failed to query item {}", query.to_string()));
    };

    repoCacheQuery query{ .id = ref.id,
                          .repo = std::nullopt,
                          .channel = ref.channel,
                          .version = ref.version.toString(),
                          .module = std::move(module),
                          .uuid = subRef,
                          .deleted = std::nullopt };
    auto item = queryItem(query);
    if (item) {
        return *item;
    }

    if (query.module != "binary") {
        return LINGLONG_ERR(item);
    }

    query.module = "runtime";
    item = queryItem(query);
    if (!item) {
        return LINGLONG_ERR(item);
    }

    LogD("fallback to runtime layer: {} {}", query.to_string(), item->commit);
    return *item;
}

auto OSTreeRepo::getLayerDir(const api::types::v1::RepositoryCacheLayersItem &layer) const noexcept
  -> utils::error::Result<package::LayerDir>
{
    LINGLONG_TRACE(fmt::format("get dir from layer {}", layer.commit));

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
    LINGLONG_TRACE(fmt::format("get dir from ref {}", ref.toString()));

    auto layer = this->getLayerItem(ref, module, subRef);
    if (!layer) {
        LogD("no such item: {}/{}:{}", ref.toString(), module, layer.error().message());
        return LINGLONG_ERR(layer);
    }

    return getLayerDir(*layer);
}

// get all module list
utils::error::Result<std::vector<std::string>> OSTreeRepo::getRemoteModuleList(
  const package::Reference &ref, const api::types::v1::Repo &repo) const noexcept
{
    LINGLONG_TRACE("get remote module list");
    auto fuzzy =
      package::FuzzyReference::create(ref.channel, ref.id, ref.version.toString(), ref.arch);
    if (!fuzzy.has_value()) {
        return LINGLONG_ERR("create fuzzy reference", fuzzy);
    }
    auto list = this->searchRemote(*fuzzy, repo, true);
    if (!list.has_value()) {
        return LINGLONG_ERR("list remote reference", fuzzy);
    }
    if (list->size() == 0) {
        return {};
    }

    std::vector<std::string> modules;
    for (const auto &ref : *list) {
        modules.emplace_back(ref.packageInfoV2Module);
    }
    std::sort(modules.begin(), modules.end());
    auto it = std::unique(modules.begin(), modules.end());
    modules.erase(it, modules.end());
    return modules;
}

// get all module list
std::vector<std::string> OSTreeRepo::getModuleList(const package::Reference &ref) const noexcept
{
    repoCacheQuery query{
        .id = ref.id,
        .repo = std::nullopt,
        .channel = ref.channel,
        .version = ref.version.toString(),
    };
    auto layers = this->cache->queryLayerItem(query);
    // 按module字母从小到大排序，提前排序以保证后面的commits比较
    std::sort(layers.begin(),
              layers.end(),
              [](const api::types::v1::RepositoryCacheLayersItem &lhs,
                 const api::types::v1::RepositoryCacheLayersItem &rhs) {
                  return lhs.info.packageInfoV2Module < rhs.info.packageInfoV2Module;
              });

    std::vector<std::string> modules;
    modules.reserve(layers.size());
    for (auto &item : layers) {
        modules.emplace_back(std::move(item.info.packageInfoV2Module));
    }
    return modules;
}

// 获取合并后的layerDir，如果没有找到则返回binary模块的layerDir
utils::error::Result<package::LayerDir>
OSTreeRepo::getMergedModuleDir(const package::Reference &ref, bool fallbackLayerDir) const noexcept
{
    LINGLONG_TRACE(fmt::format("get merge dir from ref {}", ref.toString()));
    LogD("getMergedModuleDir: {}", ref.toString());
    auto layer = this->getLayerItem(ref, "binary");
    if (!layer) {
        LogD("no such item {}/binary: {}", ref.toString(), layer.error().message());
        return LINGLONG_ERR(layer);
    }
    return getMergedModuleDir(*layer, fallbackLayerDir);
}

// 获取合并后的layerDir，如果没有找到则返回binary模块的layerDir
utils::error::Result<package::LayerDir> OSTreeRepo::getMergedModuleDir(
  const api::types::v1::RepositoryCacheLayersItem &layer, bool fallbackLayerDir) const noexcept
{
    LINGLONG_TRACE("get merge dir from layer " + layer.info.id);
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
    for (const auto &item : items.value()) {
        if (item.binaryCommit == layer.commit) {
            QDir dir = mergedDir.filePath(item.id.c_str());
            if (dir.exists()) {
                return dir.path();
            }

            qWarning().nospace() << "not exists merged dir" << dir;
        }
    }

    if (fallbackLayerDir) {
        return getLayerDir(layer);
    }
    return LINGLONG_ERR("merged doesn't exist");
}

utils::error::Result<package::LayerDir> OSTreeRepo::getMergedModuleDir(
  const package::Reference &ref, const std::vector<std::string> &loadModules) const noexcept
{
    LINGLONG_TRACE("merge modules");
    const QDir mergedDir = this->repoDir.absoluteFilePath("merged");
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
        if (layer.info.id != ref.id || layer.info.version != ref.version.toString()
            || arch != ref.arch.toStdString()) {
            continue;
        }
        if (std::find(loadModules.begin(), loadModules.end(), layer.info.packageInfoV2Module)
            == loadModules.end()) {
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
    const QString mergeID = hash.result().toHex();
    auto mergeTmp = mergedDir.filePath("tmp_" + mergeID);
    for (const auto &commit : commits) {
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
    return mergeTmp;
}

utils::error::Result<void> OSTreeRepo::mergeModules() const noexcept
{
    LINGLONG_TRACE("merge modules");
    std::error_code ec;
    const QDir mergedDir = this->repoDir.absoluteFilePath("merged");
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
                          .arg(layer.info.id.c_str(), layer.info.version.c_str(), arch.c_str())
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
            if (layer.info.packageInfoV2Module == "binary"
                || layer.info.packageInfoV2Module == "runtime") {
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
                return LINGLONG_ERR(QString("ostree_repo_checkout_at %1").arg(layer.commit.c_str()),
                                    gErr);
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
    LINGLONG_TRACE(fmt::format("rewrite desktop file {}", filePath.toStdString()));

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
    LINGLONG_TRACE(fmt::format("rewrite dbus service file {}", filePath.toStdString()));

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
    LINGLONG_TRACE(fmt::format("rewrite systemd user service {}", filePath.toStdString()));

    // Related doc: https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html
    // NOTE: The key is allowed to be repeated in the service group
    QStringList execKeys{ "ExecStart", "ExecStartPost", "ExecCondition",
                          "ExecStop",  "ExecStopPost",  "ExecReload" };
    auto file = utils::GKeyFileWrapper::New(filePath);
    if (!file) {
        return LINGLONG_ERR(file);
    }

    auto keysRet = file->getkeys(utils::GKeyFileWrapper::SystemdService);
    if (!keysRet) {
        return LINGLONG_ERR(keysRet);
    }
    const auto &keys = *keysRet;

    for (const auto &key : keys) {
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
    LINGLONG_TRACE(fmt::format("rewrite context menu{}", filePath.toStdString()));

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

utils::error::Result<package::ReferenceWithRepo>
OSTreeRepo::latestRemoteReference(const package::FuzzyReference &fuzzyRef) const noexcept
{
    LINGLONG_TRACE("get latest reference");

    auto candidates = this->matchRemoteByPriority(fuzzyRef);
    if (!candidates) {
        return LINGLONG_ERR(candidates);
    }

    auto latestPackage = candidates->getLatestPackage();
    if (!latestPackage) {
        return LINGLONG_ERR(latestPackage);
    }

    auto ref = package::Reference::fromPackageInfo(latestPackage->second);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    return package::ReferenceWithRepo{
        .repo = latestPackage->first,
        .reference = std::move(ref).value(),
    };
}

utils::error::Result<package::Reference>
OSTreeRepo::latestLocalReference(const package::FuzzyReference &fuzzyRef) const noexcept
{
    LINGLONG_TRACE("get latest local reference");

    auto ref = clearReferenceLocal(*cache, fuzzyRef, true);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    return ref;
}

utils::error::Result<std::vector<std::pair<package::Reference, package::ReferenceWithRepo>>>
OSTreeRepo::upgradableApps() const noexcept
{
    LINGLONG_TRACE("get upgradable apps");

    auto appPkgs = this->listLocalApps();
    if (!appPkgs) {
        return LINGLONG_ERR(appPkgs);
    }

    auto arch = package::Architecture::currentCPUArchitecture();
    if (!arch) {
        return LINGLONG_ERR(arch);
    }

    std::vector<std::pair<package::Reference, package::ReferenceWithRepo>> upgradeList;
    for (const auto &pkg : *appPkgs) {
        auto fuzzy = package::FuzzyReference::create(pkg.channel, pkg.id, std::nullopt, *arch);
        if (!fuzzy) {
            LogW("failed to create fuzzy reference: {}", fuzzy.error());
            continue;
        }

        auto remoteRef = this->latestRemoteReference(*fuzzy);
        if (!remoteRef) {
            LogD("Failed to find remote latest reference: {}", remoteRef.error());
            continue;
        }

        auto localRef = package::Reference::fromPackageInfo(pkg);
        if (!localRef) {
            LogW("failed to parse local reference: {}", localRef.error());
            continue;
        }

        if (remoteRef->reference.version > localRef->version) {
            upgradeList.emplace_back(
              std::make_pair(std::move(localRef).value(), std::move(remoteRef).value()));
        }
    }
    return upgradeList;
}

QDir OSTreeRepo::getEntriesDir() const noexcept
{
    return this->repoDir.absoluteFilePath("entries");
}

QDir OSTreeRepo::getDefaultSharedDir() const noexcept
{
    return this->repoDir.absoluteFilePath("entries/share");
}

QDir OSTreeRepo::getOverlayShareDir() const noexcept
{
    return this->repoDir.absoluteFilePath("entries/" LINGLONG_EXPORT_PATH);
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace linglong::repo
