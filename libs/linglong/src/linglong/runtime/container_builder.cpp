/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/api/types/v1/ApplicationConfiguration.hpp"
#include "linglong/oci-cfg-generators/builtins.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/serialize/yaml.h"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <fstream>
#include <unordered_set>

#include <fcntl.h>

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
    patches.reserve(config->permissions->binds->size());

    for (const auto &bind : *config->permissions->binds) {
        patches.push_back({
          .ociVersion = "1.0.1",
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
          }),
        });
    }

    return patches;
}

void applyJSONPatch(nlohmann::json &cfg,
                    const api::types::v1::OciConfigurationPatch &patch) noexcept
{
    LINGLONG_TRACE("apply oci runtime config patch");

    try {
        cfg = cfg.patch(patch.patch);
    } catch (...) {
        qCritical() << LINGLONG_ERRV("apply patch", std::current_exception());
        Q_ASSERT(false);
        return;
    }
}

void applyJSONFilePatch(ocppi::runtime::config::types::Config &cfg, const QFileInfo &info) noexcept
{
    LINGLONG_TRACE(QString("apply oci runtime config patch file %1").arg(info.absoluteFilePath()));

    auto patch = utils::serialize::LoadJSONFile<api::types::v1::OciConfigurationPatch>(
      info.absoluteFilePath());
    if (!patch) {
        qWarning() << LINGLONG_ERRV(patch);
        Q_ASSERT(false);
        return;
    }

    if (cfg.ociVersion != patch->ociVersion) {
        qWarning() << "ociVersion mismatched:"
                   << nlohmann::json(*patch).dump(-1, ' ', true).c_str();
        Q_ASSERT(false);
        return;
    }

    auto raw = nlohmann::json(cfg);
    applyJSONPatch(raw, *patch);
    cfg = raw.get<ocppi::runtime::config::types::Config>();
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
    if (generatorProcess.exitCode() != 0) {
        qCritical() << "generator" << info.absoluteFilePath() << "return"
                    << generatorProcess.exitCode() << "\ninput:\n"
                    << nlohmann::json(cfg).dump().c_str() << "\n\nstderr:\n"
                    << qPrintable(error);
        Q_ASSERT(false);
        return;
    }
    if (not error.isEmpty()) {
        qDebug() << "generator" << info.absoluteFilePath() << "stderr:" << error;
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
    const auto &builtins = linglong::generator::builtin_generators();
    for (const auto &info : patches) {
        if (!info.isFile()) {
            qWarning() << "info is not file:" << info.absoluteFilePath();
            continue;
        }

        if (info.completeSuffix() == "json") {
            applyJSONFilePatch(cfg, info);
            continue;
        }

        if (info.isExecutable()) {
            applyExecutablePatch(cfg, info);
            continue;
        }

        auto gen = builtins.find(info.completeBaseName().toStdString());
        if (gen == builtins.cend()) {
            qDebug() << "unsupported generator:" << info.absoluteFilePath();
            continue;
        }

        if (!gen->second->generate(cfg)) {
            qDebug() << "builtin generator failed:" << gen->first.data();
        }
    }
}

void applyPatches(ocppi::runtime::config::types::Config &cfg,
                  const std::vector<api::types::v1::OciConfigurationPatch> &patches) noexcept
{
    auto raw = nlohmann::json(cfg);
    for (const auto &patch : patches) {
        if (patch.ociVersion != cfg.ociVersion) {
            qWarning() << "ociVersion mismatched: "
                       << nlohmann::json(patch).dump(-1, ' ', true).c_str();
            continue;
        }

        applyJSONPatch(raw, patch);
    }

    cfg = raw.get<ocppi::runtime::config::types::Config>();
}

auto fixMount(ocppi::runtime::config::types::Config config) noexcept
  -> utils::error::Result<ocppi::runtime::config::types::Config>
{

    LINGLONG_TRACE("fix mount points.")

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

        for (const auto &it : tmpfsPath) {
            if (existsPath == it) {
                newTmp = false;
                break;
            }
        }

        if (newTmp) {
            tmpfsPath.push_back(existsPath);
        }
    }

    using MountType = std::remove_reference_t<decltype(mounts)>::value_type;
    auto rootBinds =
      originalRoot.entryInfoList(QDir::Dirs | QDir::Files | QDir::System | QDir::NoDotAndDotDot);
    auto pos = mounts.begin();
    for (const auto &bind : rootBinds) {
        auto mountPoint = MountType{
            .destination = ("/" + bind.fileName()).toStdString(),
            .gidMappings = {},
            .options = { { "rbind", "ro" } },
            .source = bind.absoluteFilePath().toStdString(),
            .type = "bind",
            .uidMappings = {},
        };
        if (bind.isSymLink()) {
            mountPoint.options->emplace_back("copy-symlink");
        }
        pos = mounts.insert(pos, std::move(mountPoint));
        ++pos;
    }

    for (const auto &tmpfs : tmpfsPath) {
        pos = mounts.insert(
          pos,
          MountType{
            .destination = tmpfs.mid(originalRoot.absolutePath().size()).toStdString(),
            .gidMappings = {},
            .options = { { "nodev", "nosuid", "mode=755" } },
            .source = "tmpfs",
            .type = "tmpfs",
            .uidMappings = {},
          });
        ++pos;

        auto dir = QDir{ tmpfs };
        for (const auto &rootDest :
             dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::System | QDir::NoDotAndDotDot)) {
            auto rootDestPath = rootDest.absoluteFilePath();
            auto destination = rootDestPath.mid(originalRoot.absolutePath().size());
            auto mountPoint = MountType{
                .destination = destination.toStdString(),
                .gidMappings = {},
                .options = { { "rbind", "ro" } },
                .source = rootDestPath.toStdString(),
                .type = "bind",
                .uidMappings = {},
            };
            if (rootDest.isSymLink()) {
                mountPoint.options->emplace_back("copy-symlink");
            }
            pos = mounts.insert(pos, std::move(mountPoint));
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

auto ContainerBuilder::create(const linglong::generator::ContainerCfgBuilder &cfgBuilder,
                              const QString &containerID) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create container use cfg builder");

    const auto &config = cfgBuilder.getConfig();

    return std::make_unique<Container>(config,
                                       QString::fromStdString(cfgBuilder.getAppId()),
                                       containerID,
                                       this->cli);
}

} // namespace linglong::runtime
