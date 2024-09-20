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
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/transaction.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <QDir>
#include <QProcess>

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

QString ostreeSpecFromReference(const package::Reference &ref,
                                const QString &module = "binary") noexcept
{
    if (module == "binary") {
        return QString("%1/%2/%3/%4/runtime")
          .arg(ref.channel, ref.id, ref.version.toString(), ref.arch.toString());
    }

    return QString("%1/%2/%3/%4/%5")
      .arg(ref.channel, ref.id, ref.version.toString(), ref.arch.toString(), module);
}

QString ostreeSpecFromReferenceV2(const package::Reference &ref,
                                  const QString &module = "binary",
                                  const QString &subRef = "") noexcept
{
    auto ret = QString{ "%1/%2/%3/%4/%5" }.arg(ref.channel,
                                               ref.id,
                                               ref.version.toString(),
                                               ref.arch.toString(),
                                               module);
    if (subRef.isEmpty()) {
        return ret;
    }

    return ret + "_" + subRef;
}

utils::error::Result<void> removeOstreeRef(OstreeRepo *repo, const char *ref) noexcept
{
    Q_ASSERT(ref);

    LINGLONG_TRACE("remove ostree refspec from repository");

    g_autoptr(GError) gErr = nullptr;
    g_autofree char *rev{ nullptr };

    if (ostree_repo_resolve_rev_ext(repo,
                                    ref,
                                    FALSE,
                                    OstreeRepoResolveRevExtFlags::OSTREE_REPO_RESOLVE_REV_EXT_NONE,
                                    &rev,
                                    &gErr)
        == FALSE) {
        return LINGLONG_ERR(QString{ "couldn't resolve ref %1 on local machine" }.arg(ref), gErr);
    }

    if (ostree_repo_set_ref_immediate(repo, nullptr, ref, nullptr, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_set_ref_immediate", gErr);
    }

    return LINGLONG_OK;
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

    ostree_repo_transaction_set_ref(repo, NULL, refspec, commit);

    if (ostree_repo_commit_transaction(repo, NULL, NULL, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_commit_transaction", gErr);
    }

    transaction.commit();
    return commit;
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
    const auto *minifiedJson = "minified.json";
    QFileInfo minified = layerDir.absoluteFilePath(minifiedJson);
    bool isMinified{ false };

    if (minified.exists()) {
        isMinified = true;
        auto newName =
          QDir::cleanPath(layerDir.absoluteFilePath(QString{ "../%1" }.arg(minifiedJson)));
        if (!QFile::copy(minified.absoluteFilePath(), newName)) {
            return LINGLONG_ERR("couldn't copy minified.json to parent directory");
        }
        minified.setFile(newName);
    }

    if (!layerDir.mkpath(".")) {
        Q_ASSERT(false);
        return LINGLONG_ERR(QString{ "couldn't create directory %1" }.arg(layerDir.absolutePath()));
    }

    if (!layerDir.removeRecursively()) {
        Q_ASSERT(false);
        return LINGLONG_ERR(QString{ "couldn't remove directory %1" }.arg(layerDir.absolutePath()));
    }

    auto restoreMinifiedJson =
      utils::finally::finally([isMinified,
                               refspec,
                               &layerDir,
                               currentName = minified.absoluteFilePath(),
                               originalName = layerDir.absoluteFilePath(minifiedJson),
                               &repo,
                               root] {
          if (!isMinified) {
              return;
          }

          if (!QFile::copy(currentName, originalName)) {
              qCritical() << "couldn't copy" << currentName << "to" << originalName;
              return;
          }

          if (!QFile::remove(currentName)) {
              qWarning() << "couldn't remove " << currentName
                         << ",please remove this file manually";
          }
      });

    g_autoptr(GError) gErr = nullptr;
    g_autofree char *commit{ nullptr };

    if (ostree_repo_resolve_rev_ext(repo,
                                    refspec,
                                    FALSE,
                                    OstreeRepoResolveRevExtFlags::OSTREE_REPO_RESOLVE_REV_EXT_NONE,
                                    &commit,
                                    &gErr)
        == FALSE) {
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

    auto arch = package::Architecture::currentCPUArchitecture();
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

QDir OSTreeRepo::createLayerQDir(const package::Reference &ref,
                                 const QString &module,
                                 const QString &subRef) const noexcept
{
    QDir dir =
      this->repoDir.absoluteFilePath("layers/" + ostreeSpecFromReferenceV2(ref, module, subRef));
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
    this->m_clientFactory.setServer(QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));
    this->cfg = cfg;

    transaction.commit();

    return LINGLONG_OK;
}

utils::error::Result<package::LayerDir> OSTreeRepo::importLayerDir(const package::LayerDir &dir,
                                                                   const QString &subRef) noexcept
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

    if (this->getLayerDir(*reference, QString::fromStdString(info->packageInfoV2Module), subRef)) {
        return LINGLONG_ERR(reference->toString() + " exists.", 0);
    }

    auto refspec = ostreeSpecFromReferenceV2(*reference,
                                             QString::fromStdString(info->packageInfoV2Module),
                                             subRef)
                     .toLocal8Bit();
    auto commitID = commitDirToRepo(gFile, this->ostreeRepo.get(), refspec);
    if (!commitID) {
        return LINGLONG_ERR(commitID);
    }

    transaction.addRollBack([this, &refspec]() noexcept {
        auto result = removeOstreeRef(this->ostreeRepo.get(), refspec);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    auto layerDir =
      this->createLayerQDir(*reference, QString::fromStdString(info->packageInfoV2Module), subRef);
    auto result = handleRepositoryUpdate(this->ostreeRepo.get(), layerDir, refspec);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    transaction.commit();
    return package::LayerDir{ layerDir.absolutePath() };
}

utils::error::Result<void> OSTreeRepo::push(const package::Reference &ref,
                                            const QString &module) const noexcept
{
    const qint32 HTTP_OK = 200;

    LINGLONG_TRACE("push " + ref.toString());

    auto layerDir = this->getLayerDir(ref, module);
    if (!layerDir) {
        return LINGLONG_ERR("layer not found");
    }

    auto token = [this]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("sign in");

        utils::error::Result<QString> result;

        auto env = QProcessEnvironment::systemEnvironment();
        api::client::Request_Auth auth;
        auth.setUsername(env.value("LINGLONG_USERNAME"));
        auth.setPassword(env.value("LINGLONG_PASSWORD"));
        qInfo() << "use username: " << auth.getUsername();
        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setTimeOut(10 * 60 * 1000);
        QEventLoop loop;
        QEventLoop::connect(apiClient.data(),
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

    auto taskID = [&ref, &module, this, &token]() -> utils::error::Result<QString> {
        LINGLONG_TRACE("new upload task request");

        utils::error::Result<QString> result;

        api::client::Schema_NewUploadTaskReq uploadReq;
        uploadReq.setRef(ostreeSpecFromReferenceV2(ref, module));
        uploadReq.setRepoName(QString::fromStdString(this->cfg.defaultRepo));

        auto apiClient = this->m_clientFactory.createClient();
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

    const QString tarFileName = QString("%1.tgz").arg(ref.id);
    const QString tarFilePath = QDir::cleanPath(tmpDir.filePath(tarFileName));
    QStringList args = { "-zcf", tarFilePath, "-C", layerDir->absolutePath(), "." };
    auto tarStdout = utils::command::Exec("tar", args);
    if (!tarStdout) {
        return LINGLONG_ERR(tarStdout);
    }

    auto uploadTaskResult = [this, &tarFilePath, &token, &taskID]() -> utils::error::Result<void> {
        LINGLONG_TRACE("do upload task");

        utils::error::Result<void> result;

        auto apiClient = this->m_clientFactory.createClient();
        apiClient->setTimeOut(10 * 60 * 1000);
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

    auto uploadResult = [&taskID, &token, &ref, &module, this]() -> utils::error::Result<void> {
        LINGLONG_TRACE("get upload status");

        utils::error::Result<bool> isFinished;

        auto apiClient = this->m_clientFactory.createClient();
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
                                    qDebug() << "pushing" << ref.toString() << module
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
                                              const QString &module,
                                              const QString &subRef) noexcept
{
    LINGLONG_TRACE("remove " + ref.toString());

    auto layerDir = this->getLayerDir(ref, module, subRef);
    if (!layerDir) {
        return LINGLONG_ERR("layer not found");
    }

    for (const auto &entry :
         layerDir->entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files)) {
        if (entry.fileName() == "minified.json") {
            continue;
        }

        if (entry.isDir()) {
            auto tmp = QDir{ entry.absoluteFilePath() };
            if (!tmp.removeRecursively()) {
                return LINGLONG_ERR("Failed to remove" + entry.absoluteFilePath()
                                    + "which under layer directory of" + ref.toString()
                                    + "module: " + module);
            }
            continue;
        }

        if (!QFile::remove(entry.absoluteFilePath())) {
            return LINGLONG_ERR("Failed to remove" + entry.absoluteFilePath()
                                + "which under layer directory of" + ref.toString()
                                + "module:" + module);
        }
    }

    // clean empty directories
    // from LINGLONG_ROOT/layers/main/APPID/version/arch
    // to LINGLONG_ROOT/layers

    QDir repoLayerDir(this->repoDir.absoluteFilePath("layers"));
    while (layerDir != repoLayerDir) {
        if (!layerDir->isEmpty()) {
            break;
        }
        if (!layerDir->removeRecursively()) {
            qCritical() << "Failed to remove dir: " << layerDir->absolutePath();
        }
        if (!layerDir->cdUp()) {
            qCritical() << "Failed to access the parent dir: " << layerDir->absolutePath();
            break;
        }
    }

    // remove ref from ostree repo, try new ref first
    auto refspec = ostreeSpecFromReferenceV2(ref, module, subRef).toUtf8();
    const auto *data = refspec.constData();

    auto result = removeOstreeRef(this->ostreeRepo.get(), data);
    if (result) {
        return LINGLONG_OK;
    }

    // fallback to old ref
    refspec = ostreeSpecFromReference(ref, module).toUtf8();
    result = removeOstreeRef(this->ostreeRepo.get(), refspec.constData());
    if (!result) {
        return LINGLONG_ERR(result);
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
                      const QString &module) noexcept
{
    auto refString = ostreeSpecFromReferenceV2(reference, module).toUtf8();

    LINGLONG_TRACE("pull " + refString);

    utils::Transaction transaction;
    auto *cancellable = taskContext.cancellable();

    char *refs[] = { (char *)refString.data(), nullptr };

    ostreeUserData data{ .taskContext = &taskContext };
    auto *progress = ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
    Q_ASSERT(progress != nullptr);

    g_autoptr(GError) gErr = nullptr;
    // 这里不能使用g_main_context_push_thread_default，因为会阻塞Qt的事件循环
    auto status = ostree_repo_pull(this->ostreeRepo.get(),
                                   this->cfg.defaultRepo.c_str(),
                                   refs,
                                   OSTREE_REPO_PULL_FLAGS_MIRROR,
                                   progress,
                                   cancellable,
                                   &gErr);
    ostree_async_progress_finish(progress);
    if (status == FALSE) {
        auto *progress = ostree_async_progress_new_and_connect(progress_changed, (void *)&data);
        Q_ASSERT(progress != nullptr);
        // fallback to old ref
        qWarning() << gErr->message;
        refString = ostreeSpecFromReference(reference, module).toUtf8();
        qWarning() << "fallback to module runtime, pull " << refString;

        char *oldRefs[] = { (char *)refString.data(), nullptr };

        g_clear_error(&gErr);

        status = ostree_repo_pull(this->ostreeRepo.get(),
                                  this->cfg.defaultRepo.c_str(),
                                  oldRefs,
                                  OSTREE_REPO_PULL_FLAGS_MIRROR,
                                  progress,
                                  cancellable,
                                  &gErr);
        ostree_async_progress_finish(progress);
        if (status == FALSE) {
            taskContext.reportError(LINGLONG_ERRV("ostree_repo_pull", gErr));
            return;
        }
    }

    transaction.addRollBack([this, &reference, &module]() noexcept {
        auto result = this->remove(reference, module);
        if (!result) {
            qCritical() << result.error();
            Q_ASSERT(false);
        }
    });

    auto result = handleRepositoryUpdate(this->ostreeRepo.get(),
                                         this->createLayerQDir(reference, module),
                                         refString);
    if (!result) {
        taskContext.reportError(LINGLONG_ERRV(result));
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

    auto pushBackPkgInfos = [&pkgInfos](QDir &dir) noexcept {
        QString binaryPkgInfoFilePath = dir.absoluteFilePath("binary/info.json");
        if (!QFile::exists(binaryPkgInfoFilePath)) {
            // fallback to old ref
            binaryPkgInfoFilePath = dir.absoluteFilePath("runtime/info.json");
        }
        if (QFile::exists(binaryPkgInfoFilePath)) {
            auto pkgInfo = utils::parsePackageInfo(binaryPkgInfoFilePath);
            if (pkgInfo) {
                pkgInfos.emplace_back(std::move(*pkgInfo));
            }
        }

        const QString devPkgInfoFilePath = dir.absoluteFilePath("develop/info.json");
        if (QFile::exists(devPkgInfoFilePath)) {
            auto pkgInfo = utils::parsePackageInfo(devPkgInfoFilePath);
            if (pkgInfo) {
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

auto OSTreeRepo::getLayerDir(const package::Reference &ref,
                             const QString &module,
                             const QString &subRef) const noexcept
  -> utils::error::Result<package::LayerDir>
{
    LINGLONG_TRACE("get dir of " + ref.toString());
    QDir dir =
      this->repoDir.absoluteFilePath("layers/" + ostreeSpecFromReferenceV2(ref, module, subRef));
    if (!dir.exists()) {
        dir.setPath(
          this->repoDir.absoluteFilePath("layers/" + ostreeSpecFromReference(ref, module)));
    }

    if (!dir.exists()) {
        return LINGLONG_ERR(ref.toString() + " not exist.");
    }

    return dir.absolutePath();
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace linglong::repo
