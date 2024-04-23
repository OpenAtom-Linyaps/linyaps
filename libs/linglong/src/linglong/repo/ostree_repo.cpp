/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
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
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <QDir>
#include <QProcess>
#include <QtWebSockets/QWebSocket>

#include <complex>
#include <cstddef>
#include <utility>

#include <fcntl.h>

namespace linglong::repo {

namespace {

struct ostreeUserData
{
    OSTreeRepo *repo{ nullptr };
    service::InstallTask *taskContext{ nullptr };
};

char *formatted_time_remaining_from_seconds(guint64 seconds_remaining)
{
    guint64 minutes_remaining = seconds_remaining / 60;
    guint64 hours_remaining = minutes_remaining / 60;
    guint64 days_remaining = hours_remaining / 24;

    GString *description = g_string_new(NULL);

    if (days_remaining != 0U) {
        g_string_append_printf(description, "%" G_GUINT64_FORMAT " days ", days_remaining);
    }

    if (hours_remaining != 0U) {
        g_string_append_printf(description, "%" G_GUINT64_FORMAT " hours ", hours_remaining % 24);
    }

    if (minutes_remaining != 0U) {
        g_string_append_printf(description,
                               "%" G_GUINT64_FORMAT " minutes ",
                               minutes_remaining % 60);
    }

    g_string_append_printf(description, "%" G_GUINT64_FORMAT " seconds ", seconds_remaining % 60);

    return g_string_free(description, FALSE);
}

void progress_changed(OstreeAsyncProgress *progress, gpointer user_data)
{

    auto data = static_cast<ostreeUserData *>(user_data);

    g_autofree char *status = NULL;
    gboolean caught_error = 0;
    gboolean scanning = 0;
    guint outstanding_fetches = 0;
    guint outstanding_metadata_fetches = 0;
    guint outstanding_writes = 0;
    guint n_scanned_metadata = 0;
    guint fetched_delta_parts = 0;
    guint total_delta_parts = 0;
    guint fetched_delta_part_fallbacks = 0;
    guint total_delta_part_fallbacks = 0;
    guint new_progress = 0;
    char const *formatted_bytes_sec = "0KB";

    g_autoptr(GString) buf = g_string_new("");

    ostree_async_progress_get(progress,
                              "outstanding-fetches",
                              "u",
                              &outstanding_fetches,
                              "outstanding-metadata-fetches",
                              "u",
                              &outstanding_metadata_fetches,
                              "outstanding-writes",
                              "u",
                              &outstanding_writes,
                              "caught-error",
                              "b",
                              &caught_error,
                              "scanning",
                              "u",
                              &scanning,
                              "scanned-metadata",
                              "u",
                              &n_scanned_metadata,
                              "fetched-delta-parts",
                              "u",
                              &fetched_delta_parts,
                              "total-delta-parts",
                              "u",
                              &total_delta_parts,
                              "fetched-delta-fallbacks",
                              "u",
                              &fetched_delta_part_fallbacks,
                              "total-delta-fallbacks",
                              "u",
                              &total_delta_part_fallbacks,
                              "status",
                              "s",
                              &status,
                              NULL);

    if (*status != '\0') {
        new_progress = 100;
        g_string_append(buf, status);
    } else if (caught_error) {
        auto msg = "Caught error, waiting for outstanding tasks";
        g_string_append_printf(buf, "%s", msg);
        Q_EMIT data->taskContext->updateStatus(service::InstallTask::Failed, msg);
    } else if (outstanding_fetches) {
        guint64 bytes_transferred, start_time, total_delta_part_size;
        guint fetched, metadata_fetched, requested;
        guint64 current_time = g_get_monotonic_time();
        g_autofree char *formatted_bytes_transferred = NULL;
        guint64 bytes_sec = 0;

        /* Note: This is not atomic wrt the above getter call. */
        ostree_async_progress_get(progress,
                                  "bytes-transferred",
                                  "t",
                                  &bytes_transferred,
                                  "fetched",
                                  "u",
                                  &fetched,
                                  "metadata-fetched",
                                  "u",
                                  &metadata_fetched,
                                  "requested",
                                  "u",
                                  &requested,
                                  "start-time",
                                  "t",
                                  &start_time,
                                  "total-delta-part-size",
                                  "t",
                                  &total_delta_part_size,
                                  NULL);

        formatted_bytes_transferred = g_format_size_full(bytes_transferred, (GFormatSizeFlags)0);
        /* Ignore the first second, or when we haven't transferred any
        data, since those could cause divide by zero below. */
        if ((current_time - start_time) < G_USEC_PER_SEC || bytes_transferred == 0) {
            bytes_sec = 0;
            formatted_bytes_sec = g_strdup("-");
        } else {
            bytes_sec = bytes_transferred / ((current_time - start_time) / G_USEC_PER_SEC);
            formatted_bytes_sec = g_format_size(bytes_sec);
        }

        /* Are we doing deltas?  If so, we can be more accurate */
        if (total_delta_parts > 0) {
            guint64 fetched_delta_part_size =
              ostree_async_progress_get_uint64(progress, "fetched-delta-part-size");
            g_autofree char *formatted_fetched = NULL;
            g_autofree char *formatted_total = NULL;

            /* Here we merge together deltaparts + fallbacks to avoid bloating the text UI */
            fetched_delta_parts += fetched_delta_part_fallbacks;
            total_delta_parts += total_delta_part_fallbacks;

            formatted_fetched = g_format_size(fetched_delta_part_size);
            formatted_total = g_format_size(total_delta_part_size);

            if (bytes_sec > 0) {
                guint64 est_time_remaining = 0;
                if (total_delta_part_size > fetched_delta_part_size) {
                    est_time_remaining =
                      (total_delta_part_size - fetched_delta_part_size) / bytes_sec;
                }
                g_autofree char *formatted_est_time_remaining =
                  formatted_time_remaining_from_seconds(est_time_remaining);
                /*No space between %s and remaining, since formatted_est_time_remaining has a
                trailing space */
                g_string_append_printf(buf,
                                       "Receiving delta parts: %u/%u %s/%s %s/s %sremaining",
                                       fetched_delta_parts,
                                       total_delta_parts,
                                       formatted_fetched,
                                       formatted_total,
                                       formatted_bytes_sec,
                                       formatted_est_time_remaining);
            } else {
                g_string_append_printf(buf,
                                       "Receiving delta parts: %u/%u %s/%s",
                                       fetched_delta_parts,
                                       total_delta_parts,
                                       formatted_fetched,
                                       formatted_total);
            }
        } else if ((scanning != 0) || (outstanding_metadata_fetches != 0U)) {
            new_progress += 5;
            g_object_set_data(G_OBJECT(progress), "last-was-metadata", GUINT_TO_POINTER(TRUE));
            g_string_append_printf(buf,
                                   "Receiving metadata objects: %u/(estimating) %s/s %s",
                                   metadata_fetched,
                                   formatted_bytes_sec,
                                   formatted_bytes_transferred);
        } else {
            g_string_append_printf(buf,
                                   "Receiving objects: %u%% (%u/%u) %s/s %s",
                                   (guint)((((double)fetched) / requested) * 100),
                                   fetched,
                                   requested,
                                   formatted_bytes_sec,
                                   formatted_bytes_transferred);
            new_progress = fetched * 97 / requested;
        }

        Q_EMIT data->taskContext->updateTask(fetched, requested, "pulling.");
    } else if (outstanding_writes) {
        g_string_append_printf(buf, "Writing objects: %u", outstanding_writes);
    } else {
        g_string_append_printf(buf, "Scanning metadata: %u", n_scanned_metadata);
    }
}

QString ostreeSpecFromReference(const package::Reference &ref, bool develop = false) noexcept
{

    // TODO(wurongjie) 在推送develop版的base和runtime后,应该将devel改为develop
    if (develop) {
        return QString("%1/%2/%3/%4/develop")
          .arg(ref.channel, ref.id, ref.version.toString(), ref.arch.toString());
    }

    return QString("%1/%2/%3/%4/runtime")
      .arg(ref.channel, ref.id, ref.version.toString(), ref.arch.toString());
}

utils::error::Result<void> removeOstreeRef(OstreeRepo *repo, const char *ref) noexcept
{
    Q_ASSERT(ref);

    LINGLONG_TRACE("remove ostree refspec from repository");

    g_autoptr(GError) gErr = nullptr;
    if (ostree_repo_set_ref_immediate(repo, nullptr, ref, nullptr, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_set_ref_immediate", gErr);
    }

    [[maybe_unused]] gint out_objects_total = 0;
    [[maybe_unused]] gint out_objects_pruned = 0;
    [[maybe_unused]] guint64 out_pruned_object_size_total = 0;

    if (ostree_repo_prune(repo,
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

utils::error::Result<void> commitDirToRepo(GFile *dir,
                                           OstreeRepo *repo,
                                           const char *refspec) noexcept
{
    Q_ASSERT(dir != nullptr);
    Q_ASSERT(repo != nullptr);

    LINGLONG_TRACE("commit to ostree linglong repo");

    g_autoptr(GError) gErr = nullptr;
    if (ostree_repo_prepare_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_prepare_transaction", gErr);
    }

    g_autoptr(OstreeMutableTree) mtree = ostree_mutable_tree_new();
    g_autoptr(OstreeRepoCommitModifier) modifier = nullptr;
    modifier =
      ostree_repo_commit_modifier_new(OSTREE_REPO_COMMIT_MODIFIER_FLAGS_CANONICAL_PERMISSIONS,
                                      nullptr,
                                      nullptr,
                                      nullptr);
    Q_ASSERT(modifier != nullptr);

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

    ostree_repo_transaction_set_ref(repo, NULL, refspec, commit);

    if (ostree_repo_commit_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_commit_transaction", gErr);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> handleRepositoryUpdate(OstreeRepo *repo,
                                                  QDir layerDir,
                                                  const char *refspec) noexcept
{
    LINGLONG_TRACE(QString("checkout %1 from ostree repository to layers dir").arg(refspec));

    int root = open("/", O_DIRECTORY);
    auto _ = utils::finally::finally([root]() {
        close(root);
    });

    auto path = layerDir.absolutePath();
    path = path.right(path.length() - 1);

    Q_ASSERT(layerDir.exists() == false);
    if (!layerDir.mkpath(".")) {
        Q_ASSERT(false);
    }
    if (!layerDir.removeRecursively()) {
        Q_ASSERT(false);
    }

    g_autoptr(GError) gErr = nullptr;

    g_autofree char *commit;

    if (ostree_repo_resolve_rev(repo, refspec, FALSE, &commit, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_resolve_rev", gErr);
    }

    if (ostree_repo_checkout_at(repo, nullptr, root, path.toUtf8().constData(), commit, NULL, &gErr)
        == FALSE) {
        return LINGLONG_ERR(QString("ostree_repo_checkout_at %1").arg(path), gErr);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> updateOstreeRepoConfig(OstreeRepo *repo,
                                                  const QString &remoteName,
                                                  const QString &url,
                                                  QString parent = "") noexcept
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

utils::error::Result<package::Reference> clearReferenceLocal(const package::FuzzyReference &fuzzy,
                                                             QDir layersDir) noexcept
{
    LINGLONG_TRACE("clear fuzzy reference locally");

    auto arch = package::Architecture::parse(QSysInfo::currentCpuArchitecture());
    if (fuzzy.arch) {
        arch = *fuzzy.arch;
    }

    QString channel = "main";

    if (fuzzy.channel) {
        channel = *fuzzy.channel;
    }

    QDir versionDir = layersDir.absoluteFilePath(channel + "/" + fuzzy.id);
    if (!versionDir.exists() && channel == "main") {
        // NOTE: fallback from main to linglong
        channel = "linglong";
        versionDir.setPath(layersDir.absoluteFilePath(channel + "/" + fuzzy.id));
    }
    if (!versionDir.exists()) {
        return LINGLONG_ERR("channel not found");
    }

    utils::error::Result<package::Version> foundVersion =
      LINGLONG_ERR("compatible version not found");

    auto list = versionDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto &info : list) {
        if (!info.isDir()) {
            continue;
        }

        auto availableVersion = package::Version::parse(info.fileName());
        if (!availableVersion) {
            qCritical() << "broken ostree based linglong repository detected"
                        << info.absoluteFilePath() << availableVersion.error();
            Q_ASSERT(false);
            continue;
        }

        if (!availableVersion->tweak) {
            qCritical() << "broken ostree based linglong repository detected"
                        << info.absoluteFilePath() << "tweak missing.";
            Q_ASSERT(false);
            continue;
        }

        qDebug() << "available version found:" << availableVersion->toString();

        if (!fuzzy.version) {
            foundVersion = *availableVersion;
            continue;
        }

        if (fuzzy.version->tweak) {
            if (*availableVersion != fuzzy.version) {
                continue;
            }

            foundVersion = *availableVersion;
            continue;
        }

        auto versionWithoutTweak = *availableVersion;
        versionWithoutTweak.tweak = std::nullopt;
        if (versionWithoutTweak != *fuzzy.version) {
            continue;
        }

        foundVersion = *availableVersion;
    }

    if (!foundVersion) {
        return LINGLONG_ERR(foundVersion);
    }

    auto ref = package::Reference::create(channel, fuzzy.id, *foundVersion, *arch);
    if (!ref) {
        Q_ASSERT(false);
        return LINGLONG_ERR(ref);
    }

    QDir layer = versionDir.absoluteFilePath(foundVersion->toString() + "/" + arch->toString());
    if (!layer.exists()) {
        return LINGLONG_ERR("cannot found layer " + ref->toString());
    }

    return ref;
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
        req.setArch(QSysInfo::currentCpuArchitecture());
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
              auto version = package::Version::parse(record.getVersion());
              if (!version) {
                  qWarning() << "Ignore invalid package record" << record.asJson()
                             << version.error();
                  continue;
              }

              auto arch = package::Architecture::parse(record.getArch());
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
      },
      loop.thread() == api.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

    QEventLoop::connect(
      &api,
      &api::client::ClientApi::fuzzySearchAppSignalEFull,
      &loop,
      [&](auto, auto error_type, const QString &error_str) {
          LINGLONG_TRACE("fuzzySearchAppSignalEFull");
          loop.exit();
          ref = LINGLONG_ERR(error_str, error_type);
      },
      loop.thread() == api.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

    api.fuzzySearchApp(req);
    loop.exec();

    if (!ref) {
        return LINGLONG_ERR("not found", ref);
    }

    return *ref;
}

} // namespace

QDir OSTreeRepo::getLayerQDir(const package::Reference &ref, bool develop) const noexcept
{
    return this->repoDir.absoluteFilePath("layers/" + ostreeSpecFromReference(ref, develop));
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
                       api::client::ClientApi &client) noexcept
    : cfg(cfg)
    , apiClient(client)
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
            this->ostreeRepo.reset(static_cast<OstreeRepo *>(g_steal_pointer(&ostreeRepo)));
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
}

api::types::v1::RepoConfig OSTreeRepo::getConfig() const noexcept
{
    return cfg;
}

utils::error::Result<void> OSTreeRepo::setConfig(const api::types::v1::RepoConfig &cfg) noexcept
{
    LINGLONG_TRACE("set config");

    if (cfg == this->cfg) {
        return LINGLONG_OK;
    }

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

    this->apiClient.setNewServerForAllOperations(
      QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));

    this->cfg = cfg;

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::importLayerDir(const package::LayerDir &dir) noexcept
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

    const auto isDevel = info->packageInfoModule == "develop";

    if (this->getLayerDir(*reference, isDevel)) {
        return LINGLONG_ERR(reference->toString() + " exists.");
    }

    const auto refspec = ostreeSpecFromReference(*reference, isDevel).toUtf8();

    auto result = commitDirToRepo(gFile, this->ostreeRepo.get(), refspec);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    transaction.addRollBack([this, &refspec]() noexcept {
        auto result = removeOstreeRef(this->ostreeRepo.get(), refspec);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    result = handleRepositoryUpdate(this->ostreeRepo.get(),
                                    this->getLayerQDir(*reference, isDevel),
                                    refspec);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();
    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::push(const package::Reference &ref,
                                            bool develop) const noexcept
{
    const qint32 HTTP_OK = 200;

    LINGLONG_TRACE("push " + ref.toString());

    auto token = [this]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("sign in");

        utils::error::Result<QString> result;

        auto env = QProcessEnvironment::systemEnvironment();
        api::client::Request_Auth auth;
        auth.setUsername(env.value("LINGLONG_USERNAME"));
        auth.setPassword(env.value("LINGLONG_PASSWORD"));
        qInfo() << "use auth:" << auth.asJson();

        QEventLoop loop;
        QEventLoop::connect(
          &this->apiClient,
          &api::client::ClientApi::signInSignal,
          &loop,
          [&](api::client::SignIn_200_response resp) {
              loop.exit();
              if (resp.getCode() != HTTP_OK) {
                  result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              result = resp.getData().getToken();
              return;
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::signInSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              result = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        apiClient.signIn(auth);
        loop.exec();

        if (!result) {
            return LINGLONG_ERR(result);
        }
        return result;
    }();
    if (!token) {
        return LINGLONG_ERR(token);
    }

    auto taskID = [&ref, &develop, this, &token]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("new upload task request");

        utils::error::Result<QString> result;

        api::client::Schema_NewUploadTaskReq uploadReq;
        uploadReq.setRef(ostreeSpecFromReference(ref, develop));
        uploadReq.setRepoName(QString::fromStdString(this->cfg.defaultRepo));

        QEventLoop loop;
        QEventLoop::connect(
          &this->apiClient,
          &api::client::ClientApi::newUploadTaskIDSignal,
          &loop,
          [&](const api::client::NewUploadTaskID_200_response &resp) {
              loop.exit();
              if (resp.getCode() != HTTP_OK) {
                  result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              result = resp.getData().getId();
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::newUploadTaskIDSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              result = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        apiClient.newUploadTaskID(*token, uploadReq);
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

    const QString tarFileName = QString("%1.tgz").arg(ref.id);
    const QString tarFilePath = QDir::cleanPath(tmpDir.filePath(tarFileName));
    QStringList args = { "-zcf",
                         tarFilePath,
                         "-C",
                         this->getLayerQDir(ref, develop).absolutePath(),
                         "." };
    auto tarStdout = utils::command::Exec("tar", args);
    if (!tarStdout) {
        return LINGLONG_ERR(tarStdout);
    }

    auto uploadTaskResult = [this, &tarFilePath, &token, &taskID]() -> utils::error::Result<void> {
        LINGLONG_TRACE("do upload task");

        utils::error::Result<void> result;

        QEventLoop loop;
        QEventLoop::connect(
          &this->apiClient,
          &api::client::ClientApi::uploadTaskFileSignal,
          &loop,
          [&](const api::client::Api_UploadTaskFileResp &resp) {
              loop.exit();
              if (resp.getCode() != HTTP_OK) {
                  result = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &this->apiClient,
          &api::client::ClientApi::uploadTaskFileSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              result = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        api::client::HttpFileElement file;
        file.setFileName(tarFilePath);
        file.setRequestFileName(tarFilePath);
        apiClient.uploadTaskFile(*token, *taskID, file);

        loop.exec();
        return result;
    }();
    if (!uploadTaskResult) {
        return LINGLONG_ERR(uploadTaskResult);
    }

    auto uploadResult = [&taskID, &token, &ref, &develop, this]() -> utils::error::Result<void> {
        LINGLONG_TRACE("get upload status");

        utils::error::Result<bool> isFinished;

        while (true) {
            QEventLoop loop;
            QEventLoop::connect(
              &this->apiClient,
              &api::client::ClientApi::uploadTaskInfoSignal,
              &loop,
              [&](const api::client::UploadTaskInfo_200_response &resp) {
                  loop.exit();
                  const qint32 HTTP_OK = 200;
                  if (resp.getCode() != HTTP_OK) {
                      isFinished = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                      return;
                  }
                  qDebug() << "pushing" << ref.toString() << (develop ? "develop" : "runtime")
                           << " status: " << resp.getData().getStatus();
                  if (resp.getData().getStatus() == "complete") {
                      isFinished = true;
                      return;
                  }
                  if (resp.getData().getStatus() == "failed") {
                      isFinished = LINGLONG_ERR(resp.getData().asJson());
                      return;
                  }

                  isFinished = false;
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);
            QEventLoop::connect(
              &apiClient,
              &api::client::ClientApi::uploadTaskInfoSignalEFull,
              &loop,
              [&](auto, auto error_type, const QString &error_str) {
                  loop.exit();
                  isFinished = LINGLONG_ERR(error_str, error_type);
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);
            apiClient.uploadTaskInfo(*token, *taskID);
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

utils::error::Result<void> OSTreeRepo::remove(const package::Reference &ref, bool develop) noexcept
{
    LINGLONG_TRACE("remove " + ref.toString());

    if (!this->getLayerQDir(ref, develop).removeRecursively()) {
        qCritical() << "Failed to remove layer directory of" << ref.toString()
                    << "develop:" << develop;
        Q_ASSERT(false);
    }

    auto refspec = ostreeSpecFromReference(ref, develop).toUtf8();
    const auto *data = refspec.constData();

    auto result = removeOstreeRef(this->ostreeRepo.get(), data);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

void OSTreeRepo::pull(std::shared_ptr<service::InstallTask> taskContext,
                      const package::Reference &reference,
                      bool develop) noexcept
{
    const auto refString = ostreeSpecFromReference(reference, develop).toUtf8();

    LINGLONG_TRACE("pull " + refString);

    utils::Transaction transaction;
    QTemporaryDir tmpRepoDir;
    // 初始化临时仓库
    {
        if (!tmpRepoDir.isValid()) {
            taskContext->updateStatus(service::InstallTask::Status::Failed,
                                      LINGLONG_ERRV("invalid QTemporaryDir"));
            return;
        }

        auto repo = createOstreeRepo(tmpRepoDir.path(),
                                     QString::fromStdString(this->cfg.defaultRepo),
                                     QString::fromStdString(this->cfg.repos[this->cfg.defaultRepo]),
                                     this->ostreeRepoDir().absolutePath());
        if (!repo) {
            taskContext->updateStatus(service::InstallTask::Status::Failed, LINGLONG_ERRV(repo));
            return;
        }
        g_clear_object(&(*repo));
    }
    // 因为更改了仓库配置，所以不使用初始化的值而是重新打开仓库
    g_autoptr(GError) gErr = nullptr;
    auto repoPath = g_file_new_for_path(tmpRepoDir.path().toStdString().c_str());
    auto tmpRepo = ostree_repo_new(repoPath);
    auto _ = utils::finally::finally([&]() {
        g_clear_object(&tmpRepo);
    });
    Q_ASSERT(tmpRepo != nullptr);
    if (ostree_repo_open(tmpRepo, nullptr, &gErr) != TRUE) {
        taskContext->updateStatus(service::InstallTask::Status::Failed,
                                  LINGLONG_ERRV("ostree_repo_open", gErr));
        return;
    }
    auto *cancellable = taskContext->cancellable();

    char *refs[] = { (char *)refString.data(), nullptr };

    ostreeUserData data{ .repo = this, .taskContext = taskContext.get() };
    auto *progress = ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
    Q_ASSERT(progress != nullptr);

    if (ostree_repo_pull(tmpRepo,
                         this->cfg.defaultRepo.c_str(),
                         refs,
                         OSTREE_REPO_PULL_FLAGS_MIRROR,
                         progress,
                         cancellable,
                         &gErr)
        == FALSE) {
        taskContext->updateStatus(service::InstallTask::Failed,
                                  LINGLONG_ERRV("ostree_repo_pull", gErr));
        return;
    }

    g_autofree auto *fileURLToTmpRepo =
      g_strconcat("file://", tmpRepoDir.path().toUtf8().constData(), NULL);
    GVariantBuilder builder{};
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "refs",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-static-deltas",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify-summary",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));

    g_autoptr(GVariant) options = g_variant_ref_sink(g_variant_builder_end(&builder));

    if (ostree_repo_pull_with_options(this->ostreeRepo.get(),
                                      (char *)fileURLToTmpRepo,
                                      options,
                                      NULL,
                                      cancellable,
                                      &gErr)
        == FALSE) {
        taskContext->updateStatus(service::InstallTask::Failed,
                                  LINGLONG_ERRV("ostree_repo_pull_with_options", gErr));
        return;
    };

    transaction.addRollBack([this, &reference, &develop]() noexcept {
        auto result = this->remove(reference, develop);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    auto result = handleRepositoryUpdate(this->ostreeRepo.get(),
                                         this->getLayerQDir(reference, develop),
                                         refString);
    if (!result) {
        taskContext->updateStatus(service::InstallTask::Failed, LINGLONG_ERRV(result));
        return;
    }

    transaction.commit();
}

utils::error::Result<package::Reference> OSTreeRepo::clearReference(
  const package::FuzzyReference &fuzzy, const clearReferenceOption &opts) const noexcept
{
    LINGLONG_TRACE("clear fuzzy reference " + fuzzy.toString());

    utils::error::Result<package::Reference> reference = LINGLONG_ERR("reference not exists");

    if (!opts.forceRemote) {
        reference = clearReferenceLocal(fuzzy, this->repoDir.absoluteFilePath("layers"));
        if (reference) {
            return reference;
        }

        if (!opts.fallbackToRemote) {
            return LINGLONG_ERR(reference);
        }

        qInfo() << reference.error();
        qInfo() << "fallback to Remote";
    }

    reference =
      clearReferenceRemote(fuzzy, this->apiClient, QString::fromStdString(this->cfg.defaultRepo));
    if (reference) {
        return reference;
    }

    return LINGLONG_ERR(reference);
}

utils::error::Result<std::vector<api::types::v1::PackageInfo>>
OSTreeRepo::listLocal() const noexcept
{
    std::vector<api::types::v1::PackageInfo> pkgInfos;

    QDir layersDir = this->repoDir.absoluteFilePath("layers");
    Q_ASSERT(layersDir.exists());

    auto pushBackPkgInfos = [&pkgInfos](QDir &dir) noexcept {
        QFile runtimePkgInfoFile = dir.absoluteFilePath("runtime/info.json");
        if (runtimePkgInfoFile.exists()) {
            auto pkgInfo =
              utils::serialize::LoadJSONFile<api::types::v1::PackageInfo>(runtimePkgInfoFile);
            if (pkgInfo) {
                pkgInfos.emplace_back(std::move(*pkgInfo));
            }
        }

        QFile devPkgInfoFile = dir.absoluteFilePath("develop/info.json");
        if (devPkgInfoFile.exists()) {
            auto pkgInfo =
              utils::serialize::LoadJSONFile<api::types::v1::PackageInfo>(devPkgInfoFile);
            if (!pkgInfo) {
                pkgInfos.emplace_back(std::move(*pkgInfo));
            }
        }
    };

    for (const auto &channelDir : layersDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        for (const auto &applicationInfo :
             QDir(channelDir.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            for (const auto &versionInfo : QDir(applicationInfo.absoluteFilePath())
                                             .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                for (const auto &architectureInfo :
                     QDir(versionInfo.absoluteFilePath())
                       .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    QDir architectureDir = architectureInfo.absoluteFilePath();
                    pushBackPkgInfos(architectureDir);
                }
            }
        }
    }

    return pkgInfos;
}

utils::error::Result<std::vector<api::types::v1::PackageInfo>>
OSTreeRepo::listRemote(const package::FuzzyReference &fuzzyRef) const noexcept
{
    LINGLONG_TRACE("list remote references");

    api::client::Request_FuzzySearchReq req;

    req.setRepoName(QString::fromStdString(this->cfg.defaultRepo));
    req.setAppId(fuzzyRef.id);
    if (fuzzyRef.arch) {
        req.setArch(fuzzyRef.arch->toString());
    } else {
        req.setArch(QSysInfo::currentCpuArchitecture());
    }
    if (fuzzyRef.channel) {
        req.setChannel(*fuzzyRef.channel);
    }
    if (fuzzyRef.version) {
        req.setVersion(fuzzyRef.version->toString());
    }

    utils::error::Result<std::vector<api::types::v1::PackageInfo>> pkgInfos =
      std::vector<api::types::v1::PackageInfo>{};

    QEventLoop loop;
    const qint32 HTTP_OK = 200;
    QEventLoop::connect(
      &this->apiClient,
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
              json.erase("appId");
              json["base"] = ""; // FIXME: This is werid.
              json["arch"] = nlohmann::json::array({ json["arch"] });
              auto pkgInfo = utils::serialize::LoadJSON<api::types::v1::PackageInfo>(json);
              if (!pkgInfo) {
                  qCritical() << "Ignored invalid record" << record.asJson() << pkgInfo.error();
                  continue;
              }

              pkgInfos->emplace_back(*std::move(pkgInfo));
          }
          return;
      },
      loop.thread() == this->apiClient.thread() ? Qt::AutoConnection
                                                : Qt::BlockingQueuedConnection);

    QEventLoop::connect(
      &this->apiClient,
      &api::client::ClientApi::fuzzySearchAppSignalEFull,
      &loop,
      [&](auto, auto error_type, const QString &error_str) {
          loop.exit();
          pkgInfos = LINGLONG_ERR(error_str, error_type);
      },
      loop.thread() == this->apiClient.thread() ? Qt::AutoConnection
                                                : Qt::BlockingQueuedConnection);

    this->apiClient.fuzzySearchApp(req);
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
            // NOTE: Everything in entries is directory or symbol link.
            qWarning() << "Invalid entries dir detected." << info.absoluteFilePath();
            Q_ASSERT(false);
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
}

void OSTreeRepo::unexportReference(const package::Reference &ref) noexcept
{
    auto layerQDir = this->getLayerQDir(ref);
    // if uninstall package, call removeDanglingXDGIntergation()
    if (!layerQDir.exists()) {
        removeDanglingXDGIntergation();
        return;
    }

    // if upgrade package
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
            // NOTE: Everything in entries is directory or symbol link.
            qWarning() << "Invalid entries dir detected." << info.absoluteFilePath();
            Q_ASSERT(false);
            continue;
        }

        if (!info.symLinkTarget().startsWith(layerQDir.absolutePath())) {
            continue;
        }

        if (!entriesDir.remove(it.filePath())) {
            qCritical() << "Failed to remove" << it.filePath();
            Q_ASSERT(false);
        }
    }
}

void OSTreeRepo::exportReference(const package::Reference &ref) noexcept
{
    auto entriesDir = QDir(this->repoDir.absoluteFilePath("entries/share"));
    if (!entriesDir.exists()) {
        // entries directory should exists.
        Q_ASSERT(false);
        entriesDir.mkpath(".");
    }

    auto layerDir = this->getLayerQDir(ref);
    auto layerEntriesDir = QDir(layerDir.absoluteFilePath("entries/share"));
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
}

auto OSTreeRepo::getLayerDir(const package::Reference &ref, bool develop) const noexcept
  -> utils::error::Result<package::LayerDir>
{
    LINGLONG_TRACE("get dir of " + ref.toString());
    auto dir = this->getLayerQDir(ref, develop);
    if (!dir.exists()) {
        return LINGLONG_ERR(dir.path() + " not exist.");
    }

    return dir.absolutePath();
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace linglong::repo
