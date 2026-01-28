// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "configure.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>
#include <qglobal.h>
#include <yaml-cpp/yaml.h>

#include <QCryptographicHash>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>

#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace linglong::package {

UABPackager::UABPackager(std::filesystem::path projectDir, std::filesystem::path workingDir)
{
    this->buildDir = std::move(workingDir);
    this->workDir = std::move(projectDir);

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

utils::error::Result<void> UABPackager::exclude(const std::vector<std::string> &files) noexcept
{
    LINGLONG_TRACE("append excluding files")
    for (const auto &file : files) {
        if (file.empty() || (file.at(0) != '/')) {
            return LINGLONG_ERR(std::string("invalid format, excluding file:") + file);
        }

        excludeFiles.insert(file);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::include(const std::vector<std::string> &files) noexcept
{
    LINGLONG_TRACE("append including files")
    for (const auto &file : files) {
        if (file.empty() || (file.at(0) != '/')) {
            return LINGLONG_ERR(std::string("invalid format, including file:") + file);
        }

        includeFiles.insert(file);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::pack(const std::filesystem::path &uabFilePath,
                                             bool distributedOnly) noexcept
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

    if (auto ret = packBundle(distributedOnly); !ret) {
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

utils::error::Result<std::pair<std::filesystem::path, std::filesystem::path>>
prepareSymlink(const std::filesystem::path &sourceRoot,
               const std::filesystem::path &destinationRoot,
               const std::filesystem::path &fileName,
               long maxDepth) noexcept
{
    LINGLONG_TRACE("prepare symlink")

    auto source = sourceRoot / fileName;
    auto destination = destinationRoot / fileName;

    std::error_code ec;
    while (maxDepth >= 0) {
        auto status = std::filesystem::symlink_status(source, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) {
                break;
            }

            return LINGLONG_ERR("symlink_status error", ec);
        }

        if (status.type() != std::filesystem::file_type::symlink) {
            break;
        }

        auto target = std::filesystem::read_symlink(source, ec);
        if (ec) {
            return LINGLONG_ERR("read_symlink error", ec);
        }

        // ensure parent directory of destination
        if (!std::filesystem::create_directories(destination.parent_path(), ec) && ec) {
            return LINGLONG_ERR(fmt::format("couldn't create directories {}:{}",
                                            destination.parent_path().string(),
                                            ec.message()));
        }

        // make sure the destination is not exists
        std::filesystem::remove(destination, ec);
        if (ec) {
            return LINGLONG_ERR(
              fmt::format("remove symlink {} error: {}", destination.string(), ec.message()));
        }

        std::filesystem::create_symlink(target, destination, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("create symlink {} -> {} error: {}",
                                            target.string(),
                                            destination.string(),
                                            ec.message()));
        }

        if (target.is_absolute()) {
            source = target;
            break;
        }

        source = (source.parent_path() / target).lexically_normal();
        destination = (destination.parent_path() / target).lexically_normal();
        --maxDepth;
    }

    if (maxDepth < 0) {
        return LINGLONG_ERR(fmt::format("resolve symlink {} too deep", source.string()));
    }

    return std::make_pair(std::move(source), std::move(destination));
}

utils::error::Result<void>
UABPackager::prepareExecutableBundle(const std::filesystem::path &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare layers for make a executable bundle")

    this->meta.onlyApp = true;

    std::optional<LayerDir> base;
    for (auto it = this->layers.begin(); it != this->layers.end();) {
        auto infoRet = it->info();
        if (!infoRet) {
            return LINGLONG_ERR(infoRet);
        }

        const auto &info = *infoRet;
        // the kind of old org.deepin.base,org.deepin.foundation and com.uniontech.foundation is
        // runtime
        if (info.kind == "base" || info.id == "org.deepin.base"
            || info.id == "org.deepin.foundation" || info.id == "com.uniontech.foundation") {
            base = *it;
            it = this->layers.erase(it);
            continue;
        }

        // if use custom loader, only app layer will be exported
        if (info.kind == "runtime" && !this->loader.empty()) {
            it = this->layers.erase(it);
            continue;
        }

        ++it;
    }

    if (!base) {
        return LINGLONG_ERR("couldn't find base layer");
    }

    // export layers
    auto layersDir = bundleDir / "layers";
    std::error_code ec;
    std::filesystem::create_directories(layersDir, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("couldn't create directory {}", layersDir));
    }

    auto symlinkCount = sysconf(_SC_SYMLOOP_MAX);
    if (symlinkCount < 0) {
        symlinkCount = 40;
    }

    std::filesystem::path srcLoader;
    for (const auto &layer : std::as_const(this->layers)) {
        auto infoRet = layer.info();
        if (!infoRet) {
            return LINGLONG_ERR(infoRet);
        }

        auto info = *infoRet;
        auto moduleDir = layersDir / info.id / info.packageInfoV2Module;
        std::filesystem::create_directories(moduleDir, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("couldn't create directory {}", moduleDir));
        }

        auto ret = filteringFiles(layer);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        const auto &[minified, files] = *ret;

        // first step, copy files which in layer directory
        const auto layerPath = layer.path();
        for (const auto &entry : std::filesystem::directory_iterator(layerPath, ec)) {
            if (ec) {
                return LINGLONG_ERR(fmt::format("couldn't iterate directory {}", layerPath), ec);
            }

            const auto &componentName = entry.path().filename();
            // we will apply some filters to files later, skip
            if (componentName == "files") {
                continue;
            }

            std::filesystem::copy(entry.path(),
                                  moduleDir / componentName,
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::recursive,
                                  ec);
            if (ec) {
                return LINGLONG_ERR(fmt::format("couldn't copy from {} to {}",
                                                entry.path(),
                                                moduleDir / componentName),
                                    ec);
            }
        };

        // second step, copy files which has been filtered
        auto basePath = layer.filesDirPath();
        auto moduleFilesDir = moduleDir / basePath.filename();
        if (!std::filesystem::create_directories(moduleFilesDir, ec) && ec) {
            return LINGLONG_ERR(fmt::format("couldn't create directory: {}", moduleFilesDir), ec);
        }

        if (!files.empty()) {
            struct statvfs moduleFilesDirStat{};
            struct statvfs filesStat{};

            if (statvfs(moduleFilesDir.c_str(), &moduleFilesDirStat) == -1) {
                return LINGLONG_ERR("couldn't stat module files directory: "
                                    + moduleFilesDir.string());
            }

            if (statvfs((*files.begin()).c_str(), &filesStat) == -1) {
                return LINGLONG_ERR("couldn't stat files directory: "
                                    + layer.filesDirPath().string());
            }

            const bool shouldCopy = moduleFilesDirStat.f_fsid != filesStat.f_fsid;
            for (const std::filesystem::path source : files) {
                auto sourceFile = source.lexically_relative(basePath);
                auto ret = prepareSymlink(basePath, moduleFilesDir, sourceFile, symlinkCount);
                if (!ret) {
                    return LINGLONG_ERR(ret);
                }

                const auto &[realSource, realDestination] = *ret;
                auto status = std::filesystem::symlink_status(realSource, ec);
                if (ec) {
                    // if the source file is a broken symlink, just skip it
                    if (ec == std::errc::no_such_file_or_directory) {
                        continue;
                    }

                    return LINGLONG_ERR(fmt::format("symlink_status error:{}", ec.message()));
                }

                if (std::filesystem::is_directory(realSource)) {
                    std::filesystem::create_directories(realDestination, ec);
                    if (ec) {
                        return LINGLONG_ERR(
                          fmt::format("create_directories error:{}", ec.message()));
                    }
                }

                // check destination exists or not
                // 1. multiple symlinks point to the same file
                // 2. the destination also is a symlink
                status = std::filesystem::symlink_status(realDestination, ec);
                if (!ec) {
                    // no need to check the destination symlink point to which file, just skip it
                    continue;
                }

                if (ec && ec != std::errc::no_such_file_or_directory) {
                    return LINGLONG_ERR(fmt::format("get symlink status of {} failed: {}",
                                                    realDestination.string(),
                                                    ec.message()));
                }

                if (!std::filesystem::exists(realDestination.parent_path(), ec)
                    && !std::filesystem::create_directories(realDestination.parent_path(), ec)
                    && ec) {
                    return LINGLONG_ERR(fmt::format("couldn't create directories {}:{}",
                                                    realDestination.parent_path().string(),
                                                    ec.message()));
                }

                if (shouldCopy) {
                    std::filesystem::copy(realSource, realDestination, ec);
                    if (ec) {
                        return LINGLONG_ERR(fmt::format("couldn't copy from {} to {} {}",
                                                        source,
                                                        realDestination,
                                                        ec.message()));
                    }

                    continue;
                }

                std::filesystem::create_hard_link(realSource, realDestination, ec);
                if (ec) {
                    return LINGLONG_ERR(fmt::format("couldn't link from {} to {} {}",
                                                    source,
                                                    realDestination,
                                                    ec.message()));
                }
            }
        }

        auto &layerInfoRef = this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = info, .minified = minified });

        // third step, update meta information
        if (info.kind == "app") {
            if (!this->loader.empty()) {
                srcLoader = this->loader;
            }

            auto hasMinifiedDeps = std::any_of(this->meta.layers.cbegin(),
                                               this->meta.layers.cend(),
                                               [](const api::types::v1::UabLayer &layer) {
                                                   return layer.minified;
                                               });
            // app layer is the last layer, so we could update it's packageInfo directly
            if (hasMinifiedDeps) {
                info.uuid = this->meta.uuid;
            }

            auto res = utils::writeFile(moduleDir / "info.json", nlohmann::json(info).dump());
            if (!res) {
                return LINGLONG_ERR(res);
            }
            layerInfoRef.info = info;
            continue;
        }

        // after copying runtime files, append needed files from base to runtime
        if (info.kind == "runtime") {
            const auto filesDir = base->filesDirPath();
            if (!std::filesystem::exists(filesDir, ec)) {
                return LINGLONG_ERR(fmt::format("files directory {} doesn't exist", filesDir), ec);
            }

            const auto fakePrefix =
              moduleFilesDir / "lib" / Architecture::currentCPUArchitecture().getTriplet();

            for (const std::filesystem::path file : this->neededFiles) {
                auto fileName = file.filename().string();
                auto it = std::find_if(this->blackList.begin(),
                                       this->blackList.end(),
                                       [&fileName](const std::string &entry) {
                                           return entry.rfind(fileName, 0) == 0
                                             || fileName.rfind(entry, 0) == 0;
                                       });
                if (it != this->blackList.end()) {
                    continue;
                }

                auto ret = prepareSymlink(filesDir, moduleFilesDir, file, symlinkCount);
                if (!ret) {
                    return LINGLONG_ERR(ret);
                }

                auto [source, destination] = std::move(ret).value();
                if (!std::filesystem::exists(source, ec)) {
                    if (ec) {
                        return LINGLONG_ERR(fmt::format("couldn't check file {} exists: {}",
                                                        source.string(),
                                                        ec.message()));
                    }

                    // source file must exist, it must be a regular file
                    return LINGLONG_ERR(fmt::format("file {} doesn't exist", source.string()));
                }

                auto relative = destination.lexically_relative(fakePrefix);
                if (relative.empty() || relative.string().rfind("..", 0) == 0) {
                    // override the destination file
                    destination = fakePrefix / source.filename();
                }

                if (!std::filesystem::create_directories(destination.parent_path(), ec) && ec) {
                    return LINGLONG_ERR(fmt::format("couldn't create directories {}:{}",
                                                    destination.parent_path(),
                                                    ec.message()));
                }

                std::filesystem::copy(source,
                                      destination,
                                      std::filesystem::copy_options::skip_existing,
                                      ec);
                if (ec) {
                    return LINGLONG_ERR(fmt::format("couldn't copy {} to {}, error: {}",
                                                    source.string(),
                                                    destination.string(),
                                                    ec.message()));
                }
            }

            // update runtime info.json
            auto newSize = linglong::utils::calculateDirectorySize(moduleFilesDir);
            if (!newSize) {
                return LINGLONG_ERR(newSize);
            }
            info.size = static_cast<int64_t>(*newSize);

            auto res = utils::writeFile(moduleDir / "info.json", nlohmann::json(info).dump());
            if (!res) {
                return LINGLONG_ERR(res);
            }
            layerInfoRef.info = info;
        }
    }

    if (srcLoader.empty()) {
        // default loader
        auto uabLoader = !defaultLoader.empty()
          ? defaultLoader
          : std::filesystem::path{ LINGLONG_UAB_DATA_LOCATION } / "uab-loader";
        srcLoader = uabLoader;
        if (!std::filesystem::exists(srcLoader, ec)) {
            return LINGLONG_ERR(
              fmt::format("the loader of uab application {} doesn't exist.", srcLoader));
        }
    }

    auto destLoader = bundleDir / "loader";
    if (!std::filesystem::copy_file(srcLoader, destLoader, ec)) {
        return LINGLONG_ERR(fmt::format("couldn't copy loader {} to {}", srcLoader, destLoader),
                            ec);
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

    // add extra data
    auto extraDir = bundleDir / "extra";
    std::filesystem::create_directories(extraDir, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("couldn't create directory {}", extraDir.string()));
    }

    // copy linglong-triplet-list
    auto tripletFile = base->filesDirPath() / "etc/linglong-triplet-list";
    if (std::filesystem::exists(tripletFile, ec)) {
        if (!std::filesystem::copy_file(tripletFile, extraDir / "linglong-triplet-list", ec)) {
            return LINGLONG_ERR(fmt::format("couldn't copy {} to {}",
                                            tripletFile,
                                            extraDir / "linglong-triplet-list"),
                                ec);
        }
    } else {
        LogD("{} doesn't exist in base layer", tripletFile);
    }

    // copy base profile
    auto profileFile = base->filesDirPath() / "etc/profile.d/linglong.sh";
    if (std::filesystem::exists(profileFile, ec)) {
        if (!std::filesystem::copy_file(profileFile, extraDir / "profile", ec)) {
            return LINGLONG_ERR(
              fmt::format("couldn't copy {} to {}", profileFile, extraDir / "profile"),
              ec);
        }
    } else {
        LogD("{} doesn't exist in base layer", profileFile);
    }

    // use custom loader doesn't need extra layer and ll-box any more
    if (!this->loader.empty()) {
        return LINGLONG_OK;
    }

    // copy ll-box
    auto boxBin = !defaultBox.empty() ? defaultBox : std::filesystem::path{ BINDIR } / "ll-box";
    if (!std::filesystem::exists(boxBin, ec)) {
        return LINGLONG_ERR(fmt::format("couldn't find ll-box: {}", boxBin), ec);
    }
    if (!std::filesystem::copy_file(boxBin, extraDir / "ll-box", ec)) {
        return LINGLONG_ERR(fmt::format("couldn't copy {} to {}", boxBin, extraDir / "ll-box"), ec);
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

    // check if we can use hard links for optimization (only need to check once)
    struct statvfs layersDirStat{};
    if (statvfs(layersDir.c_str(), &layersDirStat) == -1) {
        return LINGLONG_ERR("couldn't stat layers directory: " + layersDir.string());
    }

    for (const auto &layer : std::as_const(this->layers)) {
        auto info = layer.info();
        if (!info) {
            return LINGLONG_ERR(info);
        }

        LogI("info.id: {}, info.packageInfoV2Module: {}", info->id, info->packageInfoV2Module);
        auto layerPath = layer.path();
        auto modulePath = layersDir / info->id / info->packageInfoV2Module;
        std::filesystem::create_directories(modulePath, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("couldn't create directory {}", modulePath), ec);
        }

        // check if layer and target are on the same filesystem
        struct statvfs layerStat{};
        if (statvfs(layerPath.c_str(), &layerStat) == -1) {
            return LINGLONG_ERR("couldn't stat layer directory: " + layerPath.string());
        }

        const bool shouldCopy = layerStat.f_fsid != layersDirStat.f_fsid;

        if (shouldCopy) {
            // different filesystem, need to copy files
            std::filesystem::copy(layerPath,
                                  modulePath,
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::recursive,
                                  ec);
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("couldn't copy from {} to {}", layerPath, modulePath),
                  ec);
            }
        } else {
            // same filesystem, can use hard links for optimization
            // use recursive directory iterator to process all files

            // iterate through all files and directories recursively
            for (const auto &entry : std::filesystem::recursive_directory_iterator(layerPath, ec)) {
                if (ec) {
                    return LINGLONG_ERR(fmt::format("couldn't iterate directory {}", layerPath),
                                        ec);
                }

                auto relativePath = entry.path().lexically_relative(layerPath);
                auto destPath = modulePath / relativePath;

                if (entry.is_directory()) {
                    // create directory if it doesn't exist
                    std::filesystem::create_directories(destPath, ec);
                    if (ec) {
                        return LINGLONG_ERR(fmt::format("couldn't create directory {}", destPath),
                                            ec);
                    }
                    continue;
                }

                // for non-directory files, create parent directories first
                std::filesystem::create_directories(destPath.parent_path(), ec);
                if (ec) {
                    return LINGLONG_ERR(
                      fmt::format("couldn't create directories {}", destPath.parent_path()),
                      ec);
                }

                if (entry.is_symlink()) {
                    // handle symlinks - copy them directly to preserve the link target
                    std::filesystem::copy(entry.path(),
                                          destPath,
                                          std::filesystem::copy_options::copy_symlinks,
                                          ec);
                    if (ec) {
                        return LINGLONG_ERR(fmt::format("couldn't copy symlink from {} to {}",
                                                        entry.path(),
                                                        destPath),
                                            ec);
                    }
                    continue;
                }

                // regular file - try to create hard link, fallback to copy if failed
                std::filesystem::create_hard_link(entry.path(), destPath, ec);
                if (ec) {
                    // fallback to copy if hard link fails
                    std::filesystem::copy(entry.path(),
                                          destPath,
                                          std::filesystem::copy_options::copy_symlinks,
                                          ec);
                    if (ec) {
                        return LINGLONG_ERR(
                          fmt::format("couldn't copy from {} to {}", entry.path(), destPath),
                          ec);
                    }
                }
            }
        }

        // add layer info to meta
        this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = *info, .minified = false });
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packBundle(bool distributedOnly) noexcept
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

    auto ret =
      distributedOnly ? prepareDistributedBundle(bundleDir) : prepareExecutableBundle(bundleDir);
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

    return LINGLONG_OK;
}

utils::error::Result<std::pair<bool, std::unordered_set<std::string>>>
UABPackager::filteringFiles(const LayerDir &layer) const noexcept
{
    LINGLONG_TRACE("filtering files in layer directory")

    auto filesDir = layer.filesDirPath();
    std::error_code ec;
    if (!std::filesystem::exists(filesDir, ec)) {
        return LINGLONG_ERR(fmt::format("there isn't a files dir in layer {}", layer.path()), ec);
    }

    auto expandPaths = [prefix = filesDir](const std::unordered_set<std::string> &originalFiles)
      -> utils::error::Result<std::unordered_set<std::string>> {
        LINGLONG_TRACE("expand all filters")
        std::unordered_set<std::string> expandFiles;
        std::error_code ec;

        for (const auto &originalEntry : originalFiles) {
            auto entry = prefix / originalEntry.substr(1);
            auto status = std::filesystem::symlink_status(entry, ec);
            if (ec) {
                return LINGLONG_ERR(ec.message());
            }

            if (!std::filesystem::exists(status)) {
                if (ec) {
                    return LINGLONG_ERR(ec.message());
                }
                continue;
            }

            if (status.type() == std::filesystem::file_type::regular) {
                expandFiles.insert(entry);
                continue;
            }
            if (ec) {
                return LINGLONG_ERR(ec.message());
            }

            if (status.type() == std::filesystem::file_type::directory) {
                auto iterator = std::filesystem::recursive_directory_iterator(entry, ec);
                if (ec) {
                    return LINGLONG_ERR(ec.message());
                }

                for (const auto &file : iterator) {
                    if (file.is_directory(ec)) {
                        continue;
                    }

                    if (ec) {
                        return LINGLONG_ERR(ec.message());
                    }

                    expandFiles.insert(file.path().string());
                }
            }
            if (ec) {
                return LINGLONG_ERR(ec.message());
            }
        }

        return expandFiles;
    };

    auto expandedExcludesRet = expandPaths(excludeFiles);
    if (!expandedExcludesRet) {
        return LINGLONG_ERR(expandedExcludesRet);
    }
    auto &expandedExcludes = *expandedExcludesRet;

    auto expandedIncludeRet = expandPaths(includeFiles);
    if (!expandedIncludeRet) {
        return LINGLONG_ERR(expandedIncludeRet);
    }
    const auto &expandedInclude = *expandedIncludeRet;

    for (const auto &entry : expandedInclude) {
        auto it = expandedExcludes.find(entry);
        if (it != expandedExcludes.cend()) {
            expandedExcludes.erase(it);
        }
    }

    auto iterator = std::filesystem::recursive_directory_iterator(filesDir, ec);
    if (ec) {
        return LINGLONG_ERR(ec.message());
    }

    bool minified{ false };
    std::unordered_set<std::string> allFiles;
    for (const auto &file : iterator) {
        // only process regular file and symlink
        // relay on the short circuit evaluation, do not change the order of the conditions
        if (!(file.is_symlink(ec) || file.is_regular_file(ec))) {
            continue;
        }

        if (ec) {
            return LINGLONG_ERR(
              fmt::format("failed to check file {} type: {}", file.path().string(), ec.message()));
        }

        const auto &filePath = file.path();
        auto fileName = filePath.filename().string();
        auto it =
          std::find_if(this->blackList.begin(),
                       this->blackList.end(),
                       [&fileName](const std::string &entry) {
                           return entry.rfind(fileName, 0) == 0 || fileName.rfind(entry, 0) == 0;
                       });
        if (it != this->blackList.end()) {
            continue;
        }

        if (expandedExcludes.find(filePath) != expandedExcludes.cend()) {
            minified = true;
            continue;
        }

        allFiles.insert(filePath);
    }

    // append all files from include
    std::copy(expandedInclude.begin(),
              expandedInclude.end(),
              std::inserter(allFiles, allFiles.end()));

    return std::make_pair(minified, std::move(allFiles));
}

utils::error::Result<void> UABPackager::loadBlackList() noexcept
{
    LINGLONG_TRACE("load black list")
    auto blackListFile =
      std::filesystem::path{ LINGLONG_DATA_DIR } / "builder" / "uab" / "blacklist";

    std::error_code ec;
    if (!std::filesystem::exists(blackListFile, ec)) {
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to load blacklist: {}", ec.message()));
        }

        return LINGLONG_ERR(fmt::format("backlist {} doesn't exist", blackListFile.string()));
    }

    std::ifstream stream{ blackListFile };
    if (!stream.is_open()) {
        return LINGLONG_ERR(fmt::format("couldn't open blacklist: {}", blackListFile.string()));
    }

    std::string line;
    while (!stream.eof()) {
        std::getline(stream, line);
        if (line.empty() || line.rfind('#', 0) == 0) {
            continue;
        }

        auto [_, success] = this->blackList.emplace(line);
        if (!success) {
            LogW("duplicate entry: {}", line);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::loadNeededFiles() noexcept
{
    LINGLONG_TRACE("load needed files");

    auto file = workDir / "linglong" / "depends.yaml";
    if (!std::filesystem::exists(file)) {
        return LINGLONG_ERR(
          fmt::format("{} is generated by ldd-check but not found currently, it may be "
                      "deleted or ldd-check has been skipped",
                      file));
    }

    auto content = YAML::LoadFile(file.string());
    auto node = content["depends"];
    if (!node || !node.IsSequence()) {
        return LINGLONG_ERR(fmt::format("the content of {} is invalid", file.string()));
    }

    auto libs = node.as<std::vector<std::string>>();
    for (const auto &libStr : libs) {
        std::filesystem::path lib{ libStr };

        if (lib.empty()) {
            continue;
        }

        if (!lib.is_absolute()) {
            return LINGLONG_ERR(fmt::format("invalid format, lib: {}", lib.string()));
        }

        auto [_, success] = this->neededFiles.insert(lib.lexically_relative("/"));
        if (!success) {
            return LINGLONG_ERR(fmt::format("duplicate entry: {}", lib.string()));
        }
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

void UABPackager::setDefaultLoader(std::filesystem::path loader) noexcept
{
    this->defaultLoader = std::move(loader);
}

void UABPackager::setDefaultBox(std::filesystem::path box) noexcept
{
    this->defaultBox = std::move(box);
}

void UABPackager::setBundleCB(
  std::function<utils::error::Result<void>(const std::filesystem::path &,
                                           const std::filesystem::path &)> bundleCB) noexcept
{
    this->bundleCB = std::move(bundleCB);
}

} // namespace linglong::package
