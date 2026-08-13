// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "configure.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/common/strings.h"
#include "linglong/common/uab_signature.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#include <QCryptographicHash>
#include <QFile>
#include <QUuid>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <system_error>
#include <utility>

#include <sys/stat.h>

namespace linglong::package {

utils::error::Result<void> detail::copyDirectoryForDistributedBundle(
  const std::filesystem::path &source, const std::filesystem::path &destination) noexcept
{
    LINGLONG_TRACE("copy directory for distributed bundle")

    std::error_code ec;
    std::filesystem::create_directories(destination, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("couldn't create directory {}", destination), ec);
    }

    struct stat sourceStat{};
    if (stat(source.c_str(), &sourceStat) == -1) {
        return LINGLONG_ERR("couldn't stat source directory: " + source.string());
    }

    struct stat destinationStat{};
    if (stat(destination.c_str(), &destinationStat) == -1) {
        return LINGLONG_ERR("couldn't stat destination directory: " + destination.string());
    }

    if (sourceStat.st_dev != destinationStat.st_dev) {
        std::filesystem::copy(source,
                              destination,
                              std::filesystem::copy_options::copy_symlinks
                                | std::filesystem::copy_options::recursive,
                              ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("couldn't copy from {} to {}", source, destination),
                                ec);
        }
        return LINGLONG_OK;
    }

    auto iterator = std::filesystem::recursive_directory_iterator(source, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("couldn't iterate directory {}", source), ec);
    }

    const auto end = std::filesystem::recursive_directory_iterator{};
    while (iterator != end) {
        const auto &entry = *iterator;
        auto relativePath = entry.path().lexically_relative(source);
        auto destinationPath = destination / relativePath;

        // is_directory() follows symlinks, so inspect the symlink itself first. Otherwise,
        // a symlink to a directory would be exported as an empty directory.
        auto status = entry.symlink_status(ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("couldn't get status of {}", entry.path()), ec);
        }

        if (std::filesystem::is_symlink(status)) {
            std::filesystem::create_directories(destinationPath.parent_path(), ec);
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("couldn't create directories {}", destinationPath.parent_path()),
                  ec);
            }

            std::filesystem::copy(entry.path(),
                                  destinationPath,
                                  std::filesystem::copy_options::copy_symlinks,
                                  ec);
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("couldn't copy symlink from {} to {}", entry.path(), destinationPath),
                  ec);
            }
        } else if (std::filesystem::is_directory(status)) {
            std::filesystem::create_directories(destinationPath, ec);
            if (ec) {
                return LINGLONG_ERR(fmt::format("couldn't create directory {}", destinationPath),
                                    ec);
            }
        } else {
            std::filesystem::create_directories(destinationPath.parent_path(), ec);
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("couldn't create directories {}", destinationPath.parent_path()),
                  ec);
            }

            std::filesystem::create_hard_link(entry.path(), destinationPath, ec);
            if (ec) {
                std::filesystem::copy(entry.path(),
                                      destinationPath,
                                      std::filesystem::copy_options::copy_symlinks,
                                      ec);
                if (ec) {
                    return LINGLONG_ERR(
                      fmt::format("couldn't copy from {} to {}", entry.path(), destinationPath),
                      ec);
                }
            }
        }

        iterator.increment(ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("couldn't iterate directory {}", source), ec);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<std::string> detail::generateExecEntry(
  const std::vector<std::string> &command, const std::filesystem::path &prefix) noexcept
{
    LINGLONG_TRACE("generate executable UAB entry");

    if (command.empty() || command.front().empty()) {
        return LINGLONG_ERR("package command is empty");
    }

    auto executable = std::filesystem::path{ command.front() }.lexically_normal();
    if (executable.empty() || executable == "." || executable == ".."
        || common::strings::starts_with(executable.string(), "../")) {
        return LINGLONG_ERR(fmt::format("invalid package command: {}", command.front()));
    }

    auto relativeExecutable = executable.lexically_relative(prefix.lexically_normal());
    const auto commandInPrefix = !relativeExecutable.empty() && *relativeExecutable.begin() != "..";

    std::string entry = "#!/bin/sh\n"
                        "set -eu\n"
                        ": \"${LINGLONG_UAB_APPROOT:?LINGLONG_UAB_APPROOT is not set}\"\n"
                        "cd \"$LINGLONG_UAB_APPROOT\"\n"
                        "exec ";
    if (executable.is_relative()) {
        entry +=
          "\"$LINGLONG_UAB_APPROOT/bin\"/" + common::strings::quoteBashArg(executable.string());
    } else if (commandInPrefix) {
        entry += "\"$LINGLONG_UAB_APPROOT\"";
        if (relativeExecutable != ".") {
            entry += "/" + common::strings::quoteBashArg(relativeExecutable.string());
        }
    } else {
        entry += common::strings::quoteBashArg(executable.string());
    }
    for (auto it = std::next(command.cbegin()); it != command.cend(); ++it) {
        entry += " " + common::strings::quoteBashArg(*it);
    }
    entry += " \"$@\"\n";

    return entry;
}

std::string detail::generateExecLoader() noexcept
{
    return "#!/bin/sh\n"
           "set -eu\n"
           ": \"${LINGLONG_UAB_APPROOT:?LINGLONG_UAB_APPROOT is not set}\"\n"
           "exec \"$LINGLONG_UAB_APPROOT/entry.sh\" \"$@\"\n";
}

utils::error::Result<void>
detail::ensureExecEntry(const std::filesystem::path &entryPath,
                        const std::optional<std::vector<std::string>> &command,
                        const std::filesystem::path &prefix) noexcept
{
    LINGLONG_TRACE("ensure executable UAB entry");

    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::symlink_status(entryPath, ec))) {
        const auto status = std::filesystem::status(entryPath, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to check {} status", entryPath), ec);
        }
        if (!std::filesystem::is_regular_file(status)) {
            return LINGLONG_ERR(fmt::format("{} is not a regular file", entryPath));
        }

        constexpr auto executablePermissions = std::filesystem::perms::owner_exec
          | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
        if ((status.permissions() & executablePermissions) == std::filesystem::perms::none) {
            return LINGLONG_ERR(fmt::format("{} is not executable", entryPath));
        }
        return LINGLONG_OK;
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return LINGLONG_ERR(fmt::format("failed to check {}", entryPath), ec);
    }

    if (!command || command->empty()) {
        return LINGLONG_ERR("package command is required for executable bundle");
    }

    auto entry = generateExecEntry(*command, prefix);
    if (!entry) {
        return LINGLONG_ERR("failed to generate executable UAB entry", entry);
    }
    if (auto ret = utils::writeFile(entryPath, *entry); !ret) {
        return LINGLONG_ERR(fmt::format("failed to write {}", entryPath), ret);
    }

    std::filesystem::permissions(entryPath,
                                 std::filesystem::perms::owner_exec
                                   | std::filesystem::perms::group_exec
                                   | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add,
                                 ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to set {} permissions", entryPath), ec);
    }

    return LINGLONG_OK;
}

UABPackager::UABPackager(std::filesystem::path workingDir)
{
    this->buildDir = std::move(workingDir);

    meta.version = api::types::v1::Version::The1;
    meta.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

utils::error::Result<void> UABPackager::setIcon(std::filesystem::path newIcon) noexcept
{
    LINGLONG_TRACE("append icon to uab")

    std::error_code ec;
    auto status = std::filesystem::status(newIcon, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to check icon file status {}", newIcon.string()),
                            ec);
    }

    if (!std::filesystem::is_regular_file(status)) {
        return LINGLONG_ERR("icon isn't a file");
    }

    icon = std::move(newIcon);
    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::appendLayer(LayerDir layer) noexcept
{
    LINGLONG_TRACE("append layer to uab")

    if (!layer.valid()) {
        return LINGLONG_ERR(fmt::format("invalid layer directory {}", layer.path()));
    }

    layers.push_back(std::move(layer));
    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::pack(const std::filesystem::path &uabFilePath,
                                             UABPackagerMode mode) noexcept
{
    LINGLONG_TRACE("package uab")

    auto uabHeader = !defaultHeader.empty()
      ? defaultHeader
      : std::filesystem::path{ LINGLONG_UAB_DATA_LOCATION } / "uab-header";
    std::error_code ec;
    if (!std::filesystem::exists(uabHeader, ec)) {
        return LINGLONG_ERR(fmt::format("uab-header {} is missing", uabHeader), ec);
    }

    auto uabApp = buildDir / ".exported.uab";
    if (!std::filesystem::copy_file(uabHeader,
                                    uabApp,
                                    std::filesystem::copy_options::overwrite_existing,
                                    ec)) {
        return LINGLONG_ERR(
          fmt::format("couldn't copy uab header from {} to {}", uabHeader, uabApp),
          ec);
    }

    auto uab = ElfHandler::create(uabApp);
    if (!uab) {
        return LINGLONG_ERR(uab);
    }
    this->uab = std::move(uab).value();

    if (icon) {
        if (auto ret = packIcon(); !ret) {
            return ret;
        }
    }

    if (auto ret = packBundle(mode); !ret) {
        return ret;
    }

    if (auto ret = packMetaInfo(); !ret) {
        return ret;
    }

    std::filesystem::rename(uabApp, uabFilePath, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("export uab from {} to {} failed", uabApp, uabFilePath),
                            ec);
    }

    std::filesystem::permissions(uabFilePath,
                                 std::filesystem::perms::owner_exec
                                   | std::filesystem::perms::group_exec
                                   | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add,
                                 ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to set {} permissions", uabFilePath), ec);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packIcon() noexcept
{
    LINGLONG_TRACE("add icon to uab")

    std::string iconSection{ "linglong.icon" };
    if (auto ret = this->uab->addSection(iconSection, icon.value()); !ret) {
        return LINGLONG_ERR(ret);
    }

    this->meta.sections.icon = iconSection;

    return LINGLONG_OK;
}

utils::error::Result<void>
UABPackager::prepareExecutableBundle(const std::filesystem::path &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare executable bundle")

    this->meta.onlyApp = true;
    auto ret = prepareDistributedBundle(bundleDir);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    std::error_code ec;
    auto destLoader = bundleDir / "loader";
    if (!this->loader.empty()) {
        if (!std::filesystem::copy_file(this->loader, destLoader, ec)) {
            return LINGLONG_ERR(
              fmt::format("couldn't copy loader {} to {}", this->loader, destLoader),
              ec);
        }
    } else {
        const auto app =
          std::find_if(this->meta.layers.cbegin(), this->meta.layers.cend(), [](const auto &layer) {
              return layer.info.kind == "app";
          });
        if (app == this->meta.layers.cend()) {
            return LINGLONG_ERR("app layer is required for executable bundle");
        }
        const auto entryPath = bundleDir / "layers" / app->info.id / app->info.packageInfoV2Module
          / "files" / "entry.sh";
        if (auto entryRet = detail::ensureExecEntry(entryPath,
                                                    app->info.command,
                                                    std::filesystem::path{ "/opt/apps" }
                                                      / app->info.id / "files");
            !entryRet) {
            return LINGLONG_ERR(entryRet);
        }

        if (auto writeRet = utils::writeFile(destLoader, detail::generateExecLoader()); !writeRet) {
            return LINGLONG_ERR(fmt::format("failed to write {}", destLoader), writeRet);
        }
    }

    std::filesystem::permissions(destLoader,
                                 std::filesystem::perms::owner_exec
                                   | std::filesystem::perms::group_exec
                                   | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add,
                                 ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to set {} permissions", destLoader), ec);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
UABPackager::prepareDistributedBundle(const std::filesystem::path &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare distributed bundle")

    // export layers
    auto layersDir = bundleDir / "layers";
    std::error_code ec;
    std::filesystem::create_directories(layersDir, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("couldn't create directory {}", layersDir.string()), ec);
    }

    for (const auto &layer : std::as_const(this->layers)) {
        auto info = layer.info();
        if (!info) {
            return LINGLONG_ERR(info);
        }

        LogI("info.id: {}, info.packageInfoV2Module: {}", info->id, info->packageInfoV2Module);
        auto layerPath = layer.path();
        auto modulePath = layersDir / info->id / info->packageInfoV2Module;
        auto ret = detail::copyDirectoryForDistributedBundle(layerPath, modulePath);
        if (!ret) {
            return LINGLONG_ERR("couldn't prepare distributed layer", ret);
        }

        // add layer info to meta
        this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = *info, .minified = false });
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packBundle(UABPackagerMode mode) noexcept
{
    LINGLONG_TRACE("add layers to uab")

    auto bundleDir = buildDir / "bundle";
    std::error_code ec;
    if (!std::filesystem::create_directories(bundleDir, ec)) {
        return LINGLONG_ERR(fmt::format("couldn't create directory {}", bundleDir), ec);
    }

    auto bundleFile = buildDir / "bundle.ef";
    if (std::filesystem::exists(bundleFile) && !std::filesystem::remove(bundleFile, ec)) {
        return LINGLONG_ERR(fmt::format("couldn't remove file {}", bundleFile));
    }

    auto ret = mode == UABPackagerMode::Distribution ? prepareDistributedBundle(bundleDir)
                                                     : prepareExecutableBundle(bundleDir);
    if (!ret) {
        return ret;
    }

    if (bundleCB) {
        ret = bundleCB(bundleFile, bundleDir);
        if (!ret) {
            return LINGLONG_ERR("bundle error", ret);
        }
    } else {
        // https://github.com/erofs/erofs-utils/blob/b526c0d7da46b14f1328594cf1d1b2401770f59b/README#L171-L183
        if (auto ret = utils::Cmd("mkfs.erofs")
                         .exec({ "-z" + compressor,
                                 "-Efragments,dedupe,ztailpacking",
                                 "-C1048576",
                                 "-b4096", // force 4096 block size, default is page size
                                 bundleFile,
                                 bundleDir });
            !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // calculate digest
    QFile bundle{ QString::fromStdString(bundleFile.string()) };
    if (!bundle.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        return LINGLONG_ERR(fmt::format("failed to open bundle file {}", bundleFile));
    }

    QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
    if (!cryptor.addData(&bundle)) {
        return LINGLONG_ERR(fmt::format("failed to calculate digest from {}: {}",
                                        bundleFile,
                                        bundle.errorString().toStdString()));
    }
    this->meta.digest = cryptor.result().toHex().toStdString();
    const auto *bundleSection = "linglong.bundle";
    if (auto ret = this->uab->addSection(bundleSection, bundleFile); !ret) {
        return LINGLONG_ERR(ret);
    }
    this->meta.sections.bundle = bundleSection;

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packMetaInfo() noexcept
{
    LINGLONG_TRACE("add metaInfo to uab")

    auto metaFilePath = buildDir / "metaInfo.json";
    if (auto ret = utils::writeFile(metaFilePath, nlohmann::json(meta).dump()); !ret) {
        return LINGLONG_ERR(fmt::format("failed to write meta file {}", metaFilePath), ret);
    }

    const auto *metaSection = "linglong.meta";
    if (auto ret = this->uab->addSection(metaSection, metaFilePath); !ret) {
        return LINGLONG_ERR(ret);
    }

    QFile metaFile{ QString::fromStdString(metaFilePath.string()) };
    if (!metaFile.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        return LINGLONG_ERR(fmt::format("failed to open meta file {}", metaFilePath));
    }
    QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
    if (!cryptor.addData(&metaFile)) {
        return LINGLONG_ERR(fmt::format("failed to calculate digest from {}: {}",
                                        metaFilePath,
                                        metaFile.errorString().toStdString()));
    }
    const auto metaDigest = cryptor.result().toHex().toStdString();

    const auto signatureSection = std::string{ common::uab::signatureSection };
    if (auto ret = this->uab->writeSectionData(signatureSection,
                                               common::uab::digestOffset,
                                               metaDigest.data(),
                                               metaDigest.size());
        !ret) {
        return LINGLONG_ERR(
          fmt::format("failed to write digest for section {}", common::uab::metaSection),
          ret);
    }

    return LINGLONG_OK;
}

void UABPackager::setLoader(std::filesystem::path loader) noexcept
{
    this->loader = std::move(loader);
}

void UABPackager::setCompressor(std::string compressor) noexcept
{
    this->compressor = std::move(compressor);
}

void UABPackager::setDefaultHeader(std::filesystem::path header) noexcept
{
    this->defaultHeader = std::move(header);
}

void UABPackager::setBundleCB(
  std::function<utils::error::Result<void>(const std::filesystem::path &,
                                           const std::filesystem::path &)> bundleCB) noexcept
{
    this->bundleCB = std::move(bundleCB);
}

} // namespace linglong::package
