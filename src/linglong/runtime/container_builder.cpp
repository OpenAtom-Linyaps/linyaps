/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/api/types/v1/ApplicationConfiguration.hpp"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/serialize/yaml.h"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <qstandardpaths.h>

namespace linglong::runtime {

namespace {
auto getPatchesForApplication(const QString &appID) noexcept
  -> std::vector<api::types::v1::OciConfigurationPatch>
{
    auto filePath =
      QStandardPaths::locate(QStandardPaths::ConfigLocation, "linglong/" + appID + "/config.yaml");
    if (filePath.isEmpty()) {
        return {};
    }

    LINGLONG_TRACE(QString("get OCI patches for application %1").arg(appID));

    auto config =
      utils::serialize::LoadYAMLFile<api::types::v1::ApplicationConfiguration>(filePath);
    if (!config) {
        qWarning() << LINGLONG_ERRV(config);
        Q_ASSERT(false);
        return {};
    }

    if (!config->permissions) {
        return {};
    }

    if (!config->permissions->binds) {
        return {};
    }

    std::vector<api::types::v1::OciConfigurationPatch> patches;

    for (const auto &bind : *config->permissions->binds) {
        patches.push_back({ .ociVersion = "1.0.1",
                            .patch = nlohmann::json::array({
                              { "op", "add" },
                              { "path", "/mounts/-" },
                              { "value",
                                { { "source", bind.source },
                                  { "destination", bind.destination },
                                  { "options",
                                    nlohmann::json::array({
                                      "rbind",
                                      "nosuid",
                                      "nodev",
                                    }) } } },
                            }) });
    }

    return patches;
}

void applyJSONPatch(ocppi::runtime::config::types::Config &cfg,
                    const api::types::v1::OciConfigurationPatch &patch) noexcept
{
    LINGLONG_TRACE(QString("apply oci runtime config patch %1")
                     .arg(QString::fromStdString(nlohmann::json(patch).dump(-1, ' ', true))));

    if (patch.ociVersion != cfg.ociVersion) {
        qWarning() << LINGLONG_ERRV("ociVersion mismatched");
        Q_ASSERT(false);
        return;
    }

    auto raw = nlohmann::json(cfg);
    try {
        raw = raw.patch(patch.patch);
        cfg = raw.get<ocppi::runtime::config::types::Config>();
    } catch (...) {
        qCritical() << LINGLONG_ERRV("apply patch", std::current_exception());
        Q_ASSERT(false);
        return;
    }
}

void applyJSONFilePatch(ocppi::runtime::config::types::Config &cfg, const QFileInfo &info) noexcept
{
    if (!info.isFile()) {
        return;
    }

    LINGLONG_TRACE(QString("apply oci runtime config patch file %1").arg(info.absoluteFilePath()));

    if (!info.absoluteFilePath().endsWith(".json")) {
        qWarning() << LINGLONG_ERRV("file not ends with .json");
        Q_ASSERT(false);
        return;
    }

    auto patch = utils::serialize::LoadJSONFile<api::types::v1::OciConfigurationPatch>(
      info.absoluteFilePath());
    if (!patch) {
        qWarning() << LINGLONG_ERRV(patch);
        Q_ASSERT(false);
        return;
    }

    applyJSONPatch(cfg, *patch);
}

void applyExecutablePatch(ocppi::runtime::config::types::Config &cfg,
                          const QFileInfo &info) noexcept
{
    LINGLONG_TRACE(QString("process oci configuration generator %1").arg(info.absoluteFilePath()));

    QProcess generatorProcess;
    generatorProcess.setProgram(info.absoluteFilePath());
    generatorProcess.write(QByteArray::fromStdString(nlohmann::json(cfg).dump()));

    constexpr auto timeout = 200;
    if (!generatorProcess.waitForFinished(timeout)) {
        qCritical() << LINGLONG_ERRV(generatorProcess.errorString(), generatorProcess.error());
        Q_ASSERT(false);
        return;
    }

    if (generatorProcess.exitCode() != 0) {
        qCritical() << LINGLONG_ERRV("exit with error", generatorProcess.exitCode());
        Q_ASSERT(false);
        return;
    }

    auto error = generatorProcess.readAllStandardError();
    if (!error.isEmpty()) {
        qWarning() << "generator" << info.absoluteFilePath() << "stderr:" << QString(error);
    }

    auto result = generatorProcess.readAllStandardOutput();
    auto modified = utils::serialize::LoadJSON<ocppi::runtime::config::types::Config>(result);
    if (!modified) {
        qCritical() << LINGLONG_ERRV("parse stdout", modified);
        Q_ASSERT(false);
        return;
    }

    cfg = *modified;
}

void applyPatches(ocppi::runtime::config::types::Config &cfg, const QFileInfoList &patches) noexcept
{

    for (const auto &info : patches) {
        if (!info.isFile()) {
            continue;
        }

        if (info.isExecutable()) {
            applyExecutablePatch(cfg, info);
            continue;
        }

        applyJSONFilePatch(cfg, info);
    }
}

void applyPatches(ocppi::runtime::config::types::Config &cfg,
                  const std::vector<api::types::v1::OciConfigurationPatch> &patches) noexcept
{
    for (const auto &patch : patches) {
        applyJSONPatch(cfg, patch);
    }
}

void initializeMounts(ocppi::runtime::config::types::Config &config,
                      const ContainerOptions &opts) noexcept
{
    Q_ASSERT(config.mounts.has_value());

    auto &mounts = *config.mounts;

    mounts.push_back({
      .destination = "/",
      .gidMappings = std::nullopt,
      .options = { { "rbind", "ro" } },
      .source = { opts.baseDir.absoluteFilePath("files").toStdString() },
      .type = { "bind" },
      .uidMappings = std::nullopt,
    });

    if (opts.runtimeDir) {
        mounts.push_back({
          .destination = "/runtime",
          .gidMappings = std::nullopt,
          .options = { { "rbind", "ro" } },
          .source = opts.runtimeDir->absoluteFilePath("files").toStdString(),
          .type = { "bind" },
          .uidMappings = std::nullopt,
        });
    }

    if (opts.appDir) {
        mounts.push_back({
          .destination = "/opt/apps/",
          .gidMappings = std::nullopt,
          .options = { { "nodev", "nosuid", "mode=700" } },
          .source = { "tmpfs" },
          .type = { "tmpfs" },
          .uidMappings = std::nullopt,
        });

        mounts.push_back({
          .destination = QString("/opt/apps/%1/files").arg(opts.appID).toStdString(),
          .gidMappings = std::nullopt,
          .options = { { "rbind", "ro" } },
          .source = opts.appDir->absoluteFilePath("files").toStdString(),
          .type = { "bind" },
          .uidMappings = std::nullopt,
        });
    }

    {
        auto homeDir = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        Q_ASSERT(homeDir.exists());

        auto pass = [&](const QString &dir) {
            homeDir.mkpath(dir);
            Q_ASSERT(homeDir.exists(dir));
            mounts.push_back({
              .destination = homeDir.absoluteFilePath(dir).toStdString(),
              .gidMappings = std::nullopt,
              .options = { { "rbind" } },
              .source = homeDir.absoluteFilePath(dir).toStdString(),
              .type = { "bind" },
              .uidMappings = std::nullopt,
            });
        };

        pass(".");

        auto appDataDir = QDir(homeDir.absoluteFilePath(".linglong/" + opts.appID));
        appDataDir.mkpath(".");
        Q_ASSERT(appDataDir.exists());

        auto shadow = [&](const QString &hostDir, const QString &appDir) {
            homeDir.mkpath(hostDir);
            Q_ASSERT(homeDir.exists(hostDir));
            appDataDir.mkpath(appDir);
            Q_ASSERT(appDataDir.exists(appDir));

            mounts.push_back({
              .destination = homeDir.absoluteFilePath(hostDir).toStdString(),
              .gidMappings = std::nullopt,
              .options = { { "rbind" } },
              .source = appDataDir.absoluteFilePath(appDir).toStdString(),
              .type = { "bind" },
              .uidMappings = std::nullopt,
            });
        };

        // FIXME(black_desk): use XDG_* environment variables.

        // NOTE: ~/.local/share should be accessed by other applications.
        // shadow(".local/share", "share");
        shadow(".config", "config");
        shadow(".cache", "cache");
        shadow(".local/state", "state");

        if (homeDir.exists(".config/user-dirs.dirs")) {
            mounts.push_back({
              .destination = homeDir.absoluteFilePath(".config/user-dirs.dirs").toStdString(),
              .gidMappings = std::nullopt,
              .options = { { "rbind" } },
              .source = homeDir.absoluteFilePath(".config/user-dirs.dirs").toStdString(),
              .type = { "bind" },
              .uidMappings = std::nullopt,
            });
        }

        if (homeDir.exists(".config/user-dirs.locale")) {
            mounts.push_back({
              .destination = homeDir.absoluteFilePath(".config/user-dirs.locale").toStdString(),
              .gidMappings = std::nullopt,
              .options = { { "rbind" } },
              .source = homeDir.absoluteFilePath(".config/user-dirs.locale").toStdString(),
              .type = { "bind" },
              .uidMappings = std::nullopt,
            });
        }
    }
}

auto getOCIConfig(const ContainerOptions &opts) noexcept
  -> utils::error::Result<ocppi::runtime::config::types::Config>
{
    LINGLONG_TRACE("get origin OCI configuration file");

    QString containerConfigFilePath = qgetenv("LINGLONG_CONTAINER_CONFIG");
    if (containerConfigFilePath.isEmpty()) {
        containerConfigFilePath = LINGLONG_INSTALL_PREFIX "/lib/linglong/config.json";
    }

    auto config = utils::serialize::LoadJSONFile<ocppi::runtime::config::types::Config>(
      containerConfigFilePath);
    if (!config) {
        Q_ASSUME(false);
        return LINGLONG_ERR(config);
    }

    config->linux_ = config->linux_.value_or(ocppi::runtime::config::types::Linux{});
    config->linux_->uidMappings =
      config->linux_->uidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>{});
    config->linux_->uidMappings->push_back({
      .containerID = getuid(),
      .hostID = getuid(),
      .size = 1,
    });

    config->linux_->gidMappings =
      config->linux_->gidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>{});
    config->linux_->gidMappings->push_back({
      .containerID = getgid(),
      .hostID = getgid(),
      .size = 1,
    });

    initializeMounts(*config, opts);

    QDir configDotDDir = QFileInfo(containerConfigFilePath).dir().filePath("../config.d");
    Q_ASSERT(configDotDDir.exists());

    applyPatches(*config, configDotDDir.entryInfoList());

    auto appPatches = getPatchesForApplication(opts.appID);

    applyPatches(*config, appPatches);

    applyPatches(*config, opts.patches);

    Q_ASSERT(config->mounts.has_value());
    auto &mounts = *config->mounts;

    mounts.insert(mounts.end(), opts.mounts.begin(), opts.mounts.end());

    config->linux_->maskedPaths = opts.masks;

    return config;
}

} // namespace

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

auto ContainerBuilder::create(const ContainerOptions &opts) noexcept
  -> utils::error::Result<QSharedPointer<Container>>
{
    LINGLONG_TRACE("create container");

    auto config = getOCIConfig(opts);
    if (!config) {
        Q_ASSERT(false);
        return LINGLONG_ERR(config);
    }

    return QSharedPointer<Container>::create(*config, opts.containerID, this->cli);
}

} // namespace linglong::runtime
