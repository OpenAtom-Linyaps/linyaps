/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/api/types/v1/ApplicationConfiguration.hpp"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/serialize/yaml.h"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <qglobal.h>
#include <qstandardpaths.h>
#include <qtemporarydir.h>

#include <unordered_set>

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
    generatorProcess.start();
    generatorProcess.write(QByteArray::fromStdString(nlohmann::json(cfg).dump()));
    generatorProcess.closeWriteChannel();

    constexpr auto timeout = 200;
    if (!generatorProcess.waitForFinished(timeout)) {
        qCritical() << LINGLONG_ERRV(generatorProcess.errorString(), generatorProcess.error());
        Q_ASSERT(false);
        return;
    }

    auto error = generatorProcess.readAllStandardError();
    if (!error.isEmpty()) {
        qWarning() << "generator" << info.absoluteFilePath() << "stderr:" << QString(error);
    }
    if (generatorProcess.exitCode() != 0) {
        qCritical() << LINGLONG_ERRV("exit with error", generatorProcess.exitCode());
        qCritical() << "with input:" << nlohmann::json(cfg).dump().c_str();
        Q_ASSERT(false);
        return;
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

auto getOCIConfig(const ContainerOptions &opts) noexcept
  -> utils::error::Result<ocppi::runtime::config::types::Config>
{
    LINGLONG_TRACE("get origin OCI configuration file");

    QTemporaryDir dir;
    dir.setAutoRemove(false);
    QString containerConfigFilePath = qgetenv("LINGLONG_CONTAINER_CONFIG");
    if (containerConfigFilePath.isEmpty()) {
        containerConfigFilePath = LINGLONG_INSTALL_PREFIX "/lib/linglong/container/config.json";
        if (!QFile(containerConfigFilePath).exists()) {
            return LINGLONG_ERR(
              QString("The container configuration file doesn't exist: %1\n"
                      "You can specify a custom location using the LINGLONG_CONTAINER_CONFIG")
                .arg(containerConfigFilePath));
        }
    }

    auto config = utils::serialize::LoadJSONFile<ocppi::runtime::config::types::Config>(
      containerConfigFilePath);
    if (!config) {
        Q_ASSERT(false);
        return LINGLONG_ERR(config);
    }

    config->root = ocppi::runtime::config::types::Root{
        .path = opts.baseDir.absoluteFilePath("files").toStdString(),
        .readonly = true,
    };

    auto annotations = config->annotations.value_or(std::map<std::string, std::string>{});
    annotations["org.deepin.linglong.appID"] = opts.appID.toStdString();
    annotations["org.deepin.linglong.baseDir"] = opts.baseDir.absolutePath().toStdString();

    if (opts.runtimeDir) {
        annotations["org.deepin.linglong.runtimeDir"] =
          opts.runtimeDir->absolutePath().toStdString();
    }
    if (opts.appDir) {
        annotations["org.deepin.linglong.appDir"] = opts.appDir->absolutePath().toStdString();
    }
    config->annotations = std::move(annotations);

    QDir configDotDDir = QFileInfo(containerConfigFilePath).dir().filePath("config.d");
    Q_ASSERT(configDotDDir.exists());

    applyPatches(*config, configDotDDir.entryInfoList(QDir::Files));

    auto appPatches = getPatchesForApplication(opts.appID);

    applyPatches(*config, appPatches);

    applyPatches(*config, opts.patches);

    Q_ASSERT(config->mounts.has_value());
    auto &mounts = *config->mounts;

    mounts.insert(mounts.end(), opts.mounts.begin(), opts.mounts.end());

    config->linux_->maskedPaths = opts.masks;

    return config;
}

auto fixMount(ocppi::runtime::config::types::Config config) noexcept
  -> utils::error::Result<ocppi::runtime::config::types::Config>
{

    if (!config.mounts || !config.root) {
        return config;
    }

    auto originalRoot = QDir{ QString::fromStdString(config.root.value().path) };
    config.root = { { .path = "rootfs", .readonly = false } };

    auto &mounts = config.mounts.value();
    auto commonParent = [](const QString &path1, const QString &path2) {
        QString ret = path2;
        while (!path1.startsWith(ret)) {
            ret.chop(1);
        }
        if (ret.isEmpty()) {
            return ret;
        }
        while (!ret.endsWith('/')) {
            ret.chop(1);
        }
        return QDir::cleanPath(ret);
    };

    QStringList tmpfsPath;
    for (const auto &mount : mounts) {
        if (mount.destination.empty() || mount.destination.at(0) != '/') {
            continue;
        }

        auto hostSource = QDir::cleanPath(
          originalRoot.filePath(QString::fromStdString(mount.destination.substr(1))));
        if (QFileInfo::exists(hostSource)) {
            continue;
        }

        auto elem = hostSource.split(QDir::separator());
        while (!elem.isEmpty() && !QFile::exists(elem.join(QDir::separator()))) {
            elem.removeLast();
        }

        if (elem.isEmpty()) {
            qWarning() << "invalid host source:" << hostSource;
            continue;
        }

        bool newTmp{ true };
        auto existsPath = elem.join(QDir::separator());
        auto originalRootPath = QDir::cleanPath(originalRoot.absolutePath());
        if (existsPath <= originalRootPath) {
            continue;
        }

        for (auto it = tmpfsPath.cbegin(); it != tmpfsPath.cend(); ++it) {
            if (existsPath == *it) {
                newTmp = false;
                continue;
            }

            if (auto common = commonParent(existsPath, *it); common > originalRootPath) {
                newTmp = false;
                tmpfsPath.replace(it - tmpfsPath.cbegin(), common);
                break;
            }
        }

        if (newTmp) {
            tmpfsPath.push_back(existsPath);
        }
    }

    using MountType = std::remove_reference_t<decltype(mounts)>::value_type;
    auto rootBinds = originalRoot.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    auto pos = mounts.begin();
    for (const auto &bind : rootBinds) {
        pos = mounts.insert(pos,
                            MountType{ .destination = "/" + bind.fileName().toStdString(),
                                       .options = { { "rbind", "ro" } },
                                       .source = bind.absoluteFilePath().toStdString(),
                                       .type = "bind" });
        ++pos;
    }

    for (const auto &tmpfs : tmpfsPath) {
        pos = mounts.insert(
          pos,
          MountType{ .destination = tmpfs.mid(originalRoot.absolutePath().size()).toStdString(),
                     .options = { { "nodev", "nosuid", "mode=755" } },
                     .source = "tmpfs",
                     .type = "tmpfs" });
        ++pos;

        auto dir = QDir{ tmpfs };
        for (const auto &rootDest :
             dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {

            auto rootDestPath = rootDest.absoluteFilePath();
            pos = mounts.insert(
              pos,
              MountType{ .destination =
                           rootDestPath.mid(originalRoot.absolutePath().size()).toStdString(),
                         .options = { { "rbind", "ro" } },
                         .source = rootDestPath.toStdString(),
                         .type = "bind" });
            ++pos;
        }
    }

    // remove extra mount points
    std::unordered_set<std::string> dups;
    for (auto it = mounts.crbegin(); it != mounts.crend(); ++it) {
        if (dups.find(it->destination) != dups.end()) {
            mounts.erase(std::next(it).base());
            continue;
        }

        dups.insert(it->destination);
    }

    return config;
};

} // namespace

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

auto ContainerBuilder::create(const ContainerOptions &opts) noexcept
  -> utils::error::Result<QSharedPointer<Container>>
{
    LINGLONG_TRACE("create container");

    auto originalConfig = getOCIConfig(opts);
    if (!originalConfig) {
        return LINGLONG_ERR(originalConfig);
    }

    auto config = fixMount(*originalConfig);
    if (!config) {
        return LINGLONG_ERR(config);
    }

    return QSharedPointer<Container>::create(*config, opts.appID, opts.containerID, this->cli);
}

} // namespace linglong::runtime
