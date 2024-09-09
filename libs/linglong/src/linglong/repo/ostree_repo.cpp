/*
 * SPDX-FileCopyrightText: 2022-2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/task.h"
#include "linglong/repo/config.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/transaction.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <QDir>
#include <QProcess>
#include <QtWebSockets/QWebSocket>

#include <cstddef>

#include <fcntl.h>

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
    service::InstallTask *taskContext{ nullptr };
    std::string status;
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
            Q_EMIT data->taskContext->reportError(
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
        Q_EMIT data->taskContext->updateTask(static_cast<double>(data->progress),
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
                          const std::string &module = "binary",
                          const std::optional<std::string> &subRef = std::nullopt) noexcept
{
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

utils::error::Result<QString> commitDirToRepo(GFile *dir,
                                              OstreeRepo *repo,
                                              const char *refspec) noexcept
{
    Q_ASSERT(dir != nullptr);
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

    if (ostree_repo_write_directory_to_mtree(repo, dir, mtree, modifier, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_write_directory_to_mtree", gErr);
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

    if (ostree_repo_commit_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_commit_transaction", gErr);
    }

    transaction.commit();
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
    // 'main' is default channel
    fuzzy.channel = fuzzy.channel.value_or("main");

    linglong::repo::CacheRef cacheRef;
    cacheRef.id = fuzzy.id.toStdString();
    cacheRef.channel = fuzzy.channel.value().toStdString();

    const auto availablePackage = cache.searchLayerItem(cacheRef);

    if (availablePackage.empty()) {
        return LINGLONG_ERR("package not found:" % fuzzy.toString());
    }

    utils::error::Result<linglong::api::types::v1::RepositoryCacheLayersItem> foundRef =
      LINGLONG_ERR("compatible layer not found");
    for (const auto &ref : availablePackage) {
        auto ver = QString::fromStdString(ref.info.version);
        auto pkgVer = linglong::package::Version::parse(ver);
        if (!pkgVer) {
            qFatal("internal error: broken data of repo cache: %s", ver.toStdString().c_str());
        }

        qDebug() << "available layer found:" << ver;
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
    return linglong::package::Reference::create(QString::fromStdString(foundRef->info.channel),
                                                QString::fromStdString(foundRef->info.id),
                                                *ver,
                                                *arch);
}

utils::error::Result<package::Reference> clearReferenceRemote(const package::FuzzyReference &fuzzy,
                                                              api::client::ClientApi &api,
                                                              const QString &repoName) noexcept
{
    LINGLONG_TRACE("clear reference remotely");

    api::client::Request_FuzzySearchReq req;
    if (fuzzy.channel) {
        req.setChannel(*fuzzy.channel);
    }

    req.setAppId(fuzzy.id);
    if (fuzzy.version) {
        req.setVersion(fuzzy.version->toString());
    }

    if (fuzzy.arch) {
        req.setArch(fuzzy.arch->toString());
    } else {
        // NOTE: Server requires that arch is set, but why?
        req.setArch(package::Architecture::currentCPUArchitecture()->toString());
    }

    req.setRepoName(repoName);

    utils::error::Result<package::Reference> ref = LINGLONG_ERR("unknown error");

    QEventLoop loop;
    const qint32 HTTP_OK = 200;
    QEventLoop::connect(
      &api,
      &api::client::ClientApi::fuzzySearchAppSignal,
      &loop,
      [&](api::client::FuzzySearchApp_200_response resp) {
          LINGLONG_TRACE("fuzzySearchAppSignal");

          loop.exit();
          if (resp.getCode() != HTTP_OK) {
              ref = LINGLONG_ERR(resp.getMsg(), resp.getCode());
              return;
          }

          for (const auto &record : resp.getData()) {
              if (fuzzy.id != record.getAppId()) {
                  continue;
              }

              auto version = package::Version::parse(record.getVersion());
              if (!version) {
                  qWarning() << "Ignore invalid package record" << record.asJson()
                             << version.error();
                  continue;
              }

              auto arch = package::Architecture::parse(record.getArch().toStdString());
              if (!arch) {
                  qWarning() << "Ignore invalid package record" << record.asJson() << arch.error();
                  continue;
              }

              auto currentRef =
                package::Reference::create(record.getChannel(), record.getAppId(), *version, *arch);
              if (!currentRef) {
                  qWarning() << "Ignore invalid package record" << record.asJson()
                             << currentRef.error();
                  continue;
              }
              if (!ref) {
                  ref = *currentRef;
                  continue;
              }

              if (ref->version >= currentRef->version) {
                  continue;
              }

              ref = *currentRef;
          }
          return;
      });

    QEventLoop::connect(&api,
                        &api::client::ClientApi::fuzzySearchAppSignalEFull,
                        &loop,
                        [&](auto, auto error_type, const QString &error_str) {
                            LINGLONG_TRACE("fuzzySearchAppSignalEFull");
                            loop.exit();
                            ref = LINGLONG_ERR(error_str, error_type);
                        });

    api.fuzzySearchApp(req);
    loop.exec();

    if (!ref) {
        return LINGLONG_ERR("not found", ref);
    }

    return *ref;
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

QDir OSTreeRepo::createLayerQDir(const std::string &commit) const noexcept
{
    QDir dir = this->repoDir.absoluteFilePath(QString::fromStdString("layers/" + commit));
    dir.mkpath(".");
    return dir;
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
                       const api::types::v1::RepoConfig &cfg,
                       ClientFactory &clientFactory) noexcept
    : cfg(cfg)
    , m_clientFactory(clientFactory)
{
    if (!path.exists()) {
        path.mkpath(".");
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
            auto result =
              updateOstreeRepoConfig(ostreeRepo,
                                     QString::fromStdString(cfg.defaultRepo),
                                     QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));
            if (!result) {
                // when ll-cli construct this object, it has no permission to wirte ostree config
                // we can't abort here.
                qDebug() << LINGLONG_ERRV(result);
            }

            this->ostreeRepo.reset(static_cast<OstreeRepo *>(g_steal_pointer(&ostreeRepo)));
            auto ret = linglong::repo::RepoCache::create(this->repoDir.absolutePath().toStdString(),
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

    auto result = createOstreeRepo(this->ostreeRepoDir().absolutePath(),
                                   QString::fromStdString(this->cfg.defaultRepo),
                                   QString::fromStdString(this->cfg.repos[this->cfg.defaultRepo]));
    if (!result) {
        qCritical() << LINGLONG_ERRV(result);
        qFatal("abort");
    }

    this->ostreeRepo.reset(*result);

    auto ret = linglong::repo::RepoCache::create(this->repoDir.absolutePath().toStdString(),
                                                 this->cfg,
                                                 *(this->ostreeRepo));
    if (!ret) {
        qCritical() << LINGLONG_ERRV(ret);
        qFatal("abort");
    }

    this->cache = std::move(ret).value();
}

const api::types::v1::RepoConfig &OSTreeRepo::getConfig() const noexcept
{
    return cfg;
}

utils::error::Result<void>
OSTreeRepo::updateConfig(const api::types::v1::RepoConfig &newCfg) noexcept
{
    LINGLONG_TRACE("update underlying config")

    auto result = saveConfig(newCfg, this->repoDir.absoluteFilePath("config.yaml"));
    if (!result) {
        return LINGLONG_ERR(result);
    }

    utils::Transaction transaction;
    result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                    QString::fromStdString(newCfg.defaultRepo),
                                    QString::fromStdString(newCfg.repos.at(newCfg.defaultRepo)));
    transaction.addRollBack([this]() noexcept {
        auto result =
          updateOstreeRepoConfig(this->ostreeRepo.get(),
                                 QString::fromStdString(this->cfg.defaultRepo),
                                 QString::fromStdString(this->cfg.repos.at(this->cfg.defaultRepo)));
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();

    this->m_clientFactory.setServer(QString::fromStdString(newCfg.repos.at(newCfg.defaultRepo)));
    this->cfg = newCfg;

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::setConfig(const api::types::v1::RepoConfig &cfg) noexcept
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

    result = updateOstreeRepoConfig(this->ostreeRepo.get(),
                                    QString::fromStdString(cfg.defaultRepo),
                                    QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));
    if (!result) {
        return LINGLONG_ERR(result);
    }
    transaction.addRollBack([this]() noexcept {
        auto result =
          updateOstreeRepoConfig(this->ostreeRepo.get(),
                                 QString::fromStdString(this->cfg.defaultRepo),
                                 QString::fromStdString(this->cfg.repos.at(this->cfg.defaultRepo)));
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });
    this->m_clientFactory.setServer(QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));
    this->cfg = cfg;

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<package::LayerDir> OSTreeRepo::importLayerDir(
  const package::LayerDir &dir, const std::optional<std::string> &subRef) noexcept
{
    LINGLONG_TRACE("import layer dir");

    if (!dir.exists()) {
        return LINGLONG_ERR(QString("layer directory %1 not exists").arg(dir.absolutePath()));
    }

    utils::Transaction transaction;

    g_autoptr(GFile) gFile = g_file_new_for_path(dir.absolutePath().toUtf8());
    if (gFile == nullptr) {
        qFatal("g_file_new_for_path");
    }

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

    // NOTE: we save repo info in cache, if import a local layer dir, set repo to 'local'
    auto refspec =
      ostreeSpecFromReferenceV2(*reference, std::nullopt, info->packageInfoV2Module, subRef);
    auto commitID = commitDirToRepo(gFile, this->ostreeRepo.get(), refspec.c_str());
    if (!commitID) {
        return LINGLONG_ERR(commitID);
    }

    api::types::v1::RepositoryCacheLayersItem item;

    item.commit = (*commitID).toStdString();
    item.info = *info;
    item.repo = "local";

    auto layerDir = this->createLayerQDir((*commitID).toStdString());
    auto result = this->handleRepositoryUpdate(layerDir, item);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.addRollBack([this, &layerDir, &item]() noexcept {
        if (!layerDir.removeRecursively()) {
            qCritical() << "remove layer dir failed: " << layerDir.absolutePath();
            Q_ASSERT(false);
        }
        auto result = this->removeOstreeRef(item);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    transaction.commit();
    return package::LayerDir{ layerDir.absolutePath() };
}

[[nodiscard]] utils::error::Result<void> OSTreeRepo::push(const package::Reference &reference,
                                                          const std::string &module) const noexcept
{
    const auto &remoteRepo = this->cfg.defaultRepo;
    const auto &remoteURL = this->cfg.repos.at(remoteRepo);
    return pushToRemote(remoteRepo, remoteURL, reference, module);
}

utils::error::Result<void> OSTreeRepo::pushToRemote(const std::string &remoteRepo,
                                                    const std::string &url,
                                                    const package::Reference &reference,
                                                    const std::string &module) const noexcept
{
    const qint32 HTTP_OK = 200;

    LINGLONG_TRACE("push " + reference.toString());

    auto layerDir = this->getLayerDir(reference, module);
    if (!layerDir) {
        return LINGLONG_ERR("layer not found");
    }

    auto token = [this, &url]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("sign in");

        utils::error::Result<QString> result;

        auto env = QProcessEnvironment::systemEnvironment();
        api::client::Request_Auth auth;
        auth.setUsername(env.value("LINGLONG_USERNAME"));
        auth.setPassword(env.value("LINGLONG_PASSWORD"));
        qInfo() << "use username: " << auth.getUsername();
        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setNewServerForAllOperations(QString::fromStdString(url));

        const auto timeOut = 10 * 60 * 1000;
        apiClient->setTimeOut(timeOut);
        QEventLoop loop;
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::signInSignal,
                            &loop,
                            [&](const api::client::SignIn_200_response &resp) {
                                loop.exit();
                                if (resp.getCode() != HTTP_OK) {
                                    result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                                    return;
                                }
                                result = resp.getData().getToken();
                                return;
                            });
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::signInSignalEFull,
                            &loop,
                            [&](auto, auto error_type, const QString &error_str) {
                                loop.exit();
                                result = LINGLONG_ERR(error_str, error_type);
                            });

        apiClient->signIn(auth);
        loop.exec();

        if (!result) {
            return LINGLONG_ERR(result);
        }
        return result;
    }();
    if (!token) {
        return LINGLONG_ERR(token);
    }

    auto taskID =
      [&reference, &module, this, &token, &remoteRepo, &url]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("new upload task request");

        utils::error::Result<QString> result;

        api::client::Schema_NewUploadTaskReq uploadReq;
        uploadReq.setRef(
          QString::fromStdString(ostreeSpecFromReferenceV2(reference, std::nullopt, module)));
        uploadReq.setRepoName(QString::fromStdString(remoteRepo));

        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setNewServerForAllOperations(QString::fromStdString(url));
        QEventLoop loop;
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::newUploadTaskIDSignal,
                            &loop,
                            [&](const api::client::NewUploadTaskID_200_response &resp) {
                                loop.exit();
                                if (resp.getCode() != HTTP_OK) {
                                    result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                                    return;
                                }
                                result = resp.getData().getId();
                            });
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::newUploadTaskIDSignalEFull,
                            &loop,
                            [&](auto, auto error_type, const QString &error_str) {
                                loop.exit();
                                result = LINGLONG_ERR(error_str, error_type);
                            });

        apiClient->newUploadTaskID(*token, uploadReq);
        loop.exec();
        if (!result) {
            return LINGLONG_ERR(result);
        }
        return result;
    }();
    if (!taskID) {
        return LINGLONG_ERR(taskID);
    }

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

    auto uploadTaskResult =
      [this, &tarFilePath, &token, &taskID, &url]() -> utils::error::Result<void> {
        LINGLONG_TRACE("do upload task");

        utils::error::Result<void> result;

        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setNewServerForAllOperations(QString::fromStdString(url));
        const auto timeOut = 10 * 60 * 1000;
        apiClient->setTimeOut(timeOut);
        QEventLoop loop;
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::uploadTaskFileSignal,
                            &loop,
                            [&](const api::client::Api_UploadTaskFileResp &resp) {
                                loop.exit();
                                if (resp.getCode() != HTTP_OK) {
                                    result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                                    return;
                                }
                            });
        QEventLoop::connect(apiClient.data(),
                            &api::client::ClientApi::uploadTaskFileSignalEFull,
                            &loop,
                            [&](auto, auto error_type, const QString &error_str) {
                                loop.exit();
                                result = LINGLONG_ERR(error_str, error_type);
                            });

        api::client::HttpFileElement file;
        file.setFileName(tarFilePath);
        file.setRequestFileName(tarFilePath);
        apiClient->uploadTaskFile(*token, *taskID, file);

        loop.exec();
        return result;
    }();
    if (!uploadTaskResult) {
        return LINGLONG_ERR(uploadTaskResult);
    }

    auto uploadResult =
      [&taskID, &token, &reference, &module, this, &url]() -> utils::error::Result<void> {
        LINGLONG_TRACE("get upload status");

        utils::error::Result<bool> isFinished;

        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setNewServerForAllOperations(QString::fromStdString(url));
        while (true) {
            QEventLoop loop;
            QEventLoop::connect(apiClient.data(),
                                &api::client::ClientApi::uploadTaskInfoSignal,
                                &loop,
                                [&](const api::client::UploadTaskInfo_200_response &resp) {
                                    loop.exit();
                                    const qint32 HTTP_OK = 200;
                                    if (resp.getCode() != HTTP_OK) {
                                        isFinished = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                                        return;
                                    }
                                    qDebug() << "pushing" << reference.toString()
                                             << QString::fromStdString(module)
                                             << "status:" << resp.getData().getStatus();
                                    if (resp.getData().getStatus() == "complete") {
                                        isFinished = true;
                                        return;
                                    }
                                    if (resp.getData().getStatus() == "failed") {
                                        isFinished = LINGLONG_ERR(resp.getData().asJson());
                                        return;
                                    }

                                    isFinished = false;
                                });
            QEventLoop::connect(apiClient.data(),
                                &api::client::ClientApi::uploadTaskInfoSignalEFull,
                                &loop,
                                [&](auto, auto error_type, const QString &error_str) {
                                    loop.exit();
                                    isFinished = LINGLONG_ERR(error_str, error_type);
                                });
            apiClient->uploadTaskInfo(*token, *taskID);
            loop.exec();

            if (!isFinished) {
                return LINGLONG_ERR(isFinished);
            }

            if (*isFinished) {
                return LINGLONG_OK;
            }

            QThread::sleep(1);
        }

        return LINGLONG_OK;
    }();
    if (!uploadResult) {
        return LINGLONG_ERR(uploadResult);
    }

    return LINGLONG_OK;
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
    auto layerDir = this->getLayerDir(*layer);
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto ret = this->removeOstreeRef(*layer);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (!layerDir->removeRecursively()) {
        qCritical() << "Failed to remove dir: " << layerDir->absolutePath();
    }

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

void OSTreeRepo::pull(service::InstallTask &taskContext,
                      const package::Reference &reference,
                      const std::string &module) noexcept
{
    auto refString = ostreeSpecFromReferenceV2(reference, std::nullopt, module);
    LINGLONG_TRACE("pull " + QString::fromStdString(refString));

    utils::Transaction transaction;
    auto *cancellable = taskContext.cancellable();

    std::array<const char *, 2> refs{ refString.c_str(), nullptr };
    ostreeUserData data{ .taskContext = &taskContext };
    auto *progress = ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
    Q_ASSERT(progress != nullptr);

    g_autoptr(GError) gErr = nullptr;

    auto *threadContext = g_main_context_new();
    g_main_context_push_thread_default(threadContext);
    auto status = ostree_repo_pull(this->ostreeRepo.get(),
                                   this->cfg.defaultRepo.c_str(),
                                   const_cast<char **>(refs.data()), // NOLINT
                                   OSTREE_REPO_PULL_FLAGS_NONE,
                                   progress,
                                   cancellable,
                                   &gErr);
    ostree_async_progress_finish(progress);
    g_main_context_unref(threadContext);
    if (status == FALSE) {
        // fallback to old ref
        qWarning() << gErr->message;
        refString = ostreeSpecFromReference(reference, std::nullopt, module);
        qWarning() << "fallback to module runtime, pull " << QString::fromStdString(refString);

        refs[0] = refString.c_str();
        g_clear_error(&gErr);

        auto *threadContext = g_main_context_new();
        g_main_context_push_thread_default(threadContext);
        status = ostree_repo_pull(this->ostreeRepo.get(),
                                  this->cfg.defaultRepo.c_str(),
                                  const_cast<char **>(refs.data()), // NOLINT
                                  OSTREE_REPO_PULL_FLAGS_NONE,
                                  progress,
                                  cancellable,
                                  &gErr);
        ostree_async_progress_finish(progress);
        g_main_context_unref(threadContext);
        if (status == FALSE) {
            taskContext.reportError(LINGLONG_ERRV("ostree_repo_pull", gErr));
            return;
        }
    }

    g_autofree char *commit = nullptr;
    g_autoptr(GFile) root = nullptr;
    api::types::v1::RepositoryCacheLayersItem item;

    g_clear_error(&gErr);
    if (ostree_repo_read_commit(this->ostreeRepo.get(), refs[0], &root, &commit, cancellable, &gErr)
        == 0) {
        taskContext.reportError(LINGLONG_ERRV("ostree_repo_read_commit", gErr));
        return;
    }

    g_autoptr(GFile) infoFile = g_file_resolve_relative_path(root, "info.json");
    auto info = utils::parsePackageInfo(infoFile);
    if (!info) {
        taskContext.reportError(LINGLONG_ERRV(info));
        return;
    }

    item.commit = commit;
    item.info = *info;
    item.repo = this->cfg.defaultRepo;

    auto layerDir = this->createLayerQDir(item.commit);
    auto result = this->handleRepositoryUpdate(layerDir, item);

    if (!result) {
        taskContext.reportError(LINGLONG_ERRV(result));
        return;
    }

    transaction.addRollBack([this, &item, &layerDir]() noexcept {
        if (!layerDir.removeRecursively()) {
            qCritical() << "remove layer dir failed: " << layerDir.absolutePath();
            Q_ASSERT(false);
        }

        auto result = this->removeOstreeRef(item);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    transaction.commit();
}

utils::error::Result<package::Reference> OSTreeRepo::clearReference(
  const package::FuzzyReference &fuzzy, const clearReferenceOption &opts) const noexcept
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
    auto apiClient = this->m_clientFactory.createClient();
    reference =
      clearReferenceRemote(fuzzy, *apiClient, QString::fromStdString(this->cfg.defaultRepo));
    if (reference) {
        return reference;
    }

    return LINGLONG_ERR(reference);
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listLocal() const noexcept
{
    std::vector<api::types::v1::PackageInfoV2> pkgInfos;

    QDir layersDir = this->repoDir.absoluteFilePath("layers");
    Q_ASSERT(layersDir.exists());

    CacheRef cacheRef{ .id = "" };
    auto items = this->cache->searchLayerItem(cacheRef);
    for (const auto &item : items) {
        pkgInfos.emplace_back(item.info);
    }

    return pkgInfos;
}

utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>
OSTreeRepo::listRemote(const package::FuzzyReference &fuzzyRef) const noexcept
{
    LINGLONG_TRACE("list remote references");

    api::client::Request_FuzzySearchReq req;

    req.setRepoName(QString::fromStdString(this->cfg.defaultRepo));
    req.setAppId(fuzzyRef.id);
    if (fuzzyRef.arch) {
        req.setArch(fuzzyRef.arch->toString());
    } else {
        req.setArch(package::Architecture::currentCPUArchitecture()->toString());
    }
    if (fuzzyRef.channel) {
        req.setChannel(*fuzzyRef.channel);
    }
    if (fuzzyRef.version) {
        req.setVersion(fuzzyRef.version->toString());
    }

    utils::error::Result<std::vector<api::types::v1::PackageInfoV2>> pkgInfos =
      std::vector<api::types::v1::PackageInfoV2>{};
    auto apiClient = this->m_clientFactory.createClient();
    QEventLoop loop;
    const qint32 HTTP_OK = 200;
    QEventLoop::connect(
      apiClient.data(),
      &api::client::ClientApi::fuzzySearchAppSignal,
      &loop,
      [&](const api::client::FuzzySearchApp_200_response &resp) {
          loop.exit();
          if (resp.getCode() != HTTP_OK) {
              pkgInfos = LINGLONG_ERR(resp.getMsg(), resp.getCode());
              return;
          }

          for (const auto &record : resp.getData()) {
              auto json = nlohmann::json::parse(QJsonDocument(record.asJsonObject()).toJson());
              json["appid"] = json["appId"];
              json["id"] = json["appId"];
              json.erase("appId");
              json["base"] = ""; // FIXME: This is werid.
              json["arch"] = nlohmann::json::array({ json["arch"] });
              auto pkgInfo = utils::parsePackageInfo(json);
              if (!pkgInfo) {
                  qCritical() << "Ignored invalid record" << record.asJson() << pkgInfo.error();
                  continue;
              }

              pkgInfos->emplace_back(*std::move(pkgInfo));
          }
          return;
      });

    QEventLoop::connect(apiClient.data(),
                        &api::client::ClientApi::fuzzySearchAppSignalEFull,
                        &loop,
                        [&](auto, auto error_type, const QString &error_str) {
                            loop.exit();
                            pkgInfos = LINGLONG_ERR(error_str, error_type);
                        });

    apiClient->fuzzySearchApp(req);
    loop.exec();

    if (!pkgInfos) {
        return LINGLONG_ERR(pkgInfos);
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
        Q_ASSERT(false);
        qCritical() << "Failed to unexport" << ref.toString() << "layer not exists.";
        return;
    }

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
    bool shouldExport = true;

    [&ref, this, &shouldExport]() {
        // Check if we should export the application we just pulled to system.

        auto pkgInfos = this->listLocal();
        if (!pkgInfos) {
            qCritical() << pkgInfos.error();
            Q_ASSERT(false);
            return;
        }

        std::vector<package::Reference> refs;

        for (const auto &localInfo : *pkgInfos) {
            if (QString::fromStdString(localInfo.id) != ref.id) {
                continue;
            }

            auto localRef = package::Reference::fromPackageInfo(localInfo);
            if (!localRef) {
                qCritical() << localRef.error();
                Q_ASSERT(false);
                continue;
            }

            if (localRef->version > ref.version) {
                qInfo() << localRef->toString() << "exists, we should not export" << ref.toString();
                shouldExport = false;
                return;
            }

            refs.push_back(*localRef);
        }

        for (const auto &ref : refs) {
            this->unexportReference(ref);
        }
    }();

    if (!shouldExport) {
        return;
    }

    auto entriesDir = QDir(this->repoDir.absoluteFilePath("entries/share"));
    if (!entriesDir.exists()) {
        // entries directory should exists.
        Q_ASSERT(false);
        entriesDir.mkpath(".");
    }

    auto layerDir = this->getLayerDir(ref);
    if (!layerDir) {
        Q_ASSERT(false);
        qCritical() << QString("Failed to export %1:").arg(ref.toString())
                    << "layer directory not exists.";
        return;
    }
    if (!layerDir->exists()) { }

    auto layerEntriesDir = QDir(layerDir->absoluteFilePath("entries/share"));
    if (!layerEntriesDir.exists()) {
        Q_ASSERT(false);
        qCritical() << QString("Failed to export %1:").arg(ref.toString()) << layerEntriesDir
                    << "not exists.";
        return;
    }

    QDirIterator it(layerEntriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto info = it.fileInfo();
        if (info.isDir()) {
            continue;
        }
        // In KDE environment, every desktop should own the executable permission
        // We just set the file permission to 0755 here.
        if (info.suffix() == "desktop") {
            if (!QFile::setPermissions(info.absoluteFilePath(),
                                       QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                         | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                         | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                         | QFileDevice::ExeOther)) {
                qCritical() << "Failed to chmod" << info.absoluteFilePath();
                Q_ASSERT(false);
            }
        }

        const auto parentDirForLinkPath =
          layerEntriesDir.relativeFilePath(it.fileInfo().dir().absolutePath());

        if (!entriesDir.mkpath(parentDirForLinkPath)) {
            qCritical() << "Failed to mkpath" << entriesDir.absoluteFilePath(parentDirForLinkPath);
            Q_ASSERT(false);
        }

        QDir parentDir(entriesDir.absoluteFilePath(parentDirForLinkPath));
        const auto from = entriesDir.absoluteFilePath(parentDirForLinkPath) + "/" + it.fileName();
        const auto to = parentDir.relativeFilePath(info.absoluteFilePath());

        if (!QFile::link(to, from)) {
            qCritical() << "Failed to create link" << to << "->" << from;
            Q_ASSERT(false);
        }
    }
    this->updateSharedInfo();
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
                + applicationDir.absolutePath() + ": " + *ret;
        }
    }

    // 更新 mime type database
    if (mimeDataDir.exists()) {
        auto ret = utils::command::Exec("update-mime-database", { mimeDataDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update mime type database in "
                + mimeDataDir.absolutePath() + ": " + *ret;
        }
    }

    // 更新 glib-2.0/schemas
    if (glibSchemasDir.exists()) {
        auto ret = utils::command::Exec("glib-compile-schemas", { glibSchemasDir.absolutePath() });
        if (!ret) {
            qWarning() << "warning: failed to update schemas in " + glibSchemasDir.absolutePath()
                + ": " + *ret;
        }
    }
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
OSTreeRepo::getLayerItem(const package::Reference &ref,
                         const std::string &module,
                         const std::optional<std::string> &subRef) const noexcept
{
    LINGLONG_TRACE("get latest layer of " + ref.toString());

    linglong::repo::CacheRef cacheRef{ .id = ref.id.toStdString(),
                                       .repo = std::nullopt,
                                       .channel = ref.channel.toStdString(),
                                       .version = ref.version.toString().toStdString(),
                                       .module = module,
                                       .uuid = subRef };
    auto items = this->cache->searchLayerItem(cacheRef);
    auto count = items.size();
    if (count > 1) {
        return LINGLONG_ERR("ambiguous ref, matched count: " + QString::number(count));
    }

    if (count == 0) {
        // fallback to runtime module
        cacheRef.module = "runtime";
        items = this->cache->searchLayerItem(cacheRef);
        if (items.size() > 1) {
            return LINGLONG_ERR("ambiguous ref, matched count: " + QString::number(count));
        } else if (items.size() == 0) {
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

    qCritical() << QString::fromStdString(layer.info.id)
                << QString::fromStdString(layer.info.version);
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
        return LINGLONG_ERR(layer);
    }

    return getLayerDir(*layer);
}

utils::error::Result<void> OSTreeRepo::migrate() noexcept
{
    // if (cache.isLayersEmpty()) { retrun LINGLONG_OK; }
    // TODO: implement this
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace linglong::repo
