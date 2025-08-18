// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "configure.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/command/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"

#include <qglobal.h>
#include <yaml-cpp/yaml.h>

#include <QCryptographicHash>
#include <QStandardPaths>

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

Q_LOGGING_CATEGORY(uab_packager, "packager.uab")

elfHelper::elfHelper(QByteArray path, int fd, Elf *ptr)
    : filePath(std::move(path))
    , elfFd(fd)
    , e(ptr)
{
}

elfHelper::elfHelper(elfHelper &&other) noexcept
    : filePath(std::move(other).filePath)
    , elfFd(other.elfFd)
    , e(other.e)
{
    other.e = nullptr;
    other.elfFd = -1;
}

elfHelper &elfHelper::operator=(elfHelper &&other) noexcept
{
    if (*this == other) {
        return *this;
    }

    this->e = other.e;
    this->elfFd = other.elfFd;
    this->filePath = std::move(other).filePath;

    other.e = nullptr;
    other.elfFd = -1;

    return *this;
}

elfHelper::~elfHelper()
{
    if (elfFd == -1) {
        return;
    }

    elf_end(e);
    ::close(elfFd);
}

utils::error::Result<elfHelper> elfHelper::create(const QByteArray &filePath) noexcept
{
    LINGLONG_TRACE("create elfHelper");

    if (!QFileInfo::exists(filePath)) {
        auto ret = filePath + "doesn't exists";
        return LINGLONG_ERR(ret);
    }

    // TODO: use libelf
    // auto fd = ::open(filePath.data(), O_RDWR);
    // if (fd == -1) {
    //     return LINGLONG_ERR(strerror(errno));
    // }

    // auto *elf = elf_begin(fd, ELF_C_RDWR, nullptr);
    // if (elf == nullptr) {
    //     return LINGLONG_ERR(
    //       QString{ "%1 not usable: %2" }.arg(QString{ filePath }).arg(elf_errmsg(-1)));
    // }

    return elfHelper{ filePath, -1, nullptr };
}

utils::error::Result<void> elfHelper::addNewSection(const QByteArray &sectionName,
                                                    const QFileInfo &dataFile,
                                                    const QStringList &flags) const noexcept
{
    LINGLONG_TRACE(QString{ "add section:%1" }.arg(QString{ sectionName }))

    auto args =
      QStringList{ QString{ "--add-section" },
                   QString{ "%1=%2" }.arg(QString{ sectionName }, dataFile.absoluteFilePath()) };

    if (!flags.empty()) {
        args.append({ "--set-section-flags", flags.join(QString{ "," }) });
    }

    args.append({ this->elfPath(), this->elfPath() });
    auto ret = utils::command::Cmd("objcopy").exec(args);
    if (!ret) {
        return LINGLONG_ERR(ret.error());
    }

    return LINGLONG_OK;
}

UABPackager::UABPackager(const QDir &projectDir, QDir workingDir)
{
    if (!workingDir.mkpath(".")) {
        qFatal("can't create working directory: %s", qPrintable(workingDir.absolutePath()));
    }

    this->buildDir = std::move(workingDir);
    this->workDir = projectDir.absolutePath().toStdString();

    meta.version = api::types::v1::Version::The1;
    meta.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

UABPackager::~UABPackager()
{
    auto env = ::qgetenv("LINGLONG_UAB_DEBUG");
    if (!env.isEmpty()) {
        auto buildDirPath = buildDir.absolutePath();
        auto suffix = QUuid::createUuid().toString(QUuid::StringFormat::Id128).left(6);
        auto randomName = buildDirPath + "-" + suffix;
        if (QFile::rename(buildDirPath, randomName)) {
            return;
        }

        qWarning() << "couldn't rename build directory" << buildDirPath << "to" << randomName
                   << ",try to remove it.";
    }

    if (!buildDir.removeRecursively()) {
        qCCritical(uab_packager) << "couldn't remove build directory, please remove it manually.";
    }
}

utils::error::Result<void> UABPackager::setIcon(const QFileInfo &newIcon) noexcept
{
    LINGLONG_TRACE("append icon to uab")

    if (!newIcon.exists()) {
        return LINGLONG_ERR("icon doesn't exists");
    }

    if (!newIcon.isFile()) {
        return LINGLONG_ERR("icon isn't a file");
    }

    icon = newIcon;
    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::appendLayer(const LayerDir &layer) noexcept
{
    LINGLONG_TRACE("append layer to uab")

    if (!layer.exists()) {
        return LINGLONG_ERR("icon doesn't exists");
    }

    layers.append(layer);
    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::exclude(const std::vector<std::string> &files) noexcept
{
    LINGLONG_TRACE("append excluding files")
    for (const auto &file : files) {
        if (file.empty() || (file.at(0) != '/')) {
            return LINGLONG_ERR("invalid format, excluding file:" + QString::fromStdString(file));
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
            return LINGLONG_ERR("invalid format, including file:" + QString::fromStdString(file));
        }

        includeFiles.insert(file);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::pack(const QString &uabFilePath,
                                             bool distributedOnly) noexcept
{
    LINGLONG_TRACE("package uab")

    QString uabHeader = !defaultHeader.isEmpty()
      ? defaultHeader
      : QDir{ LINGLONG_UAB_DATA_LOCATION }.filePath("uab-header");
    if (!QFileInfo::exists(uabHeader)) {
        return LINGLONG_ERR("uab-header is missing");
    }

    auto uabApp = buildDir.filePath(".exported.uab");
    if (QFileInfo::exists(uabApp) && !QFile::remove(uabApp)) {
        return LINGLONG_ERR("couldn't remove uab cache");
    }

    if (!QFile::copy(uabHeader, uabApp)) {
        return LINGLONG_ERR(
          QString{ "couldn't copy uab header from %1 to %2" }.arg(uabHeader, uabApp));
    }

    auto uab = elfHelper::create(uabApp.toLocal8Bit());
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

    auto exportPath = uabFilePath;

    if (QFileInfo::exists(exportPath) && !QFile::remove(exportPath)) {
        return LINGLONG_ERR("couldn't remove previous uab file");
    }

    if (!QFile::rename(this->uab.elfPath(), exportPath)) {
        return LINGLONG_ERR(
          QString{ "export uab from %1 to %2 failed" }.arg(QString{ this->uab.elfPath() },
                                                           exportPath));
    }

    if (!QFile::setPermissions(exportPath,
                               QFile::permissions(exportPath) | QFile::ExeOwner | QFile::ExeGroup
                                 | QFile::ExeOther)) {
        return LINGLONG_ERR("couldn't set executable permission to uab");
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packIcon() noexcept
{
    LINGLONG_TRACE("add icon to uab")

    auto iconAchieve = this->uab.parentDir().absoluteFilePath("icon.a");
    if (auto ret = utils::command::Cmd("ar").exec({ "q", iconAchieve, icon->absoluteFilePath() });
        !ret) {
        return LINGLONG_ERR(ret);
    }

    QByteArray iconSection{ "linglong.icon" };
    if (auto ret = this->uab.addNewSection(iconSection, QFileInfo{ iconAchieve }); !ret) {
        return LINGLONG_ERR(ret);
    }

    this->meta.sections.icon = iconSection.toStdString();

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

            return LINGLONG_ERR(
              QString{ "symlink_status error:%1" }.arg(QString::fromStdString(ec.message())));
        }

        if (status.type() != std::filesystem::file_type::symlink) {
            break;
        }

        auto target = std::filesystem::read_symlink(source, ec);
        if (ec) {
            return LINGLONG_ERR(
              QString{ "read_symlink error:%1" }.arg(QString::fromStdString(ec.message())));
        }

        // ensure parent directory of destination
        if (!std::filesystem::create_directories(destination.parent_path(), ec) && ec) {
            return LINGLONG_ERR(
              "couldn't create directories "
              % QString::fromStdString(destination.parent_path().string() + ":" + ec.message()));
        }

        std::filesystem::create_symlink(target, destination, ec);
        while (ec) {
            if (ec != std::errc::file_exists) {
                return LINGLONG_ERR(
                  QString{ "create_symlink error:%1" }.arg(QString::fromStdString(ec.message())));
            }

            // check symlink target is the same as the original target
            auto status = std::filesystem::symlink_status(destination, ec);
            if (ec) {
                return LINGLONG_ERR(
                  QString{ "symlink_status %1 error: %2" }.arg(destination.string().c_str(),
                                                               ec.message().c_str()));
            }

            // destination already exists and is not a symlink
            if (status.type() != std::filesystem::file_type::symlink) {
                break;
            }

            auto curTarget = std::filesystem::read_symlink(destination, ec);
            if (ec) {
                return LINGLONG_ERR(
                  QString{ "read_symlink %1 error: %2" }.arg(destination.string().c_str(),
                                                             ec.message().c_str()));
            }

            if (curTarget != target) {
                return LINGLONG_ERR(
                  QString{ "symlink target is not the same as the original target" }
                  % "original target: " % QString::fromStdString(target.string())
                  % "current target: " % QString::fromStdString(curTarget.string()));
            }

            break;
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
        return LINGLONG_ERR(QString{ "resolve symlink %1 too deep" }.arg(source.c_str()));
    }

    return std::make_pair(std::move(source), std::move(destination));
}

utils::error::Result<void> UABPackager::prepareExecutableBundle(const QDir &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare layers for make a executable bundle")

    this->meta.onlyApp = true;

    std::optional<LayerDir> base;
    for (auto it = this->layers.begin(); it != this->layers.end();) {
        auto infoRet = it->info();
        if (!infoRet) {
            return LINGLONG_ERR(QString{ "failed export layer %1:" }.arg(it->absolutePath()),
                                infoRet.error());
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

        if (info.kind == "runtime") {
            // if use custom loader, only app layer will be exported
            if (!this->loader.isEmpty()) {
                it = this->layers.erase(it);
                continue;
            }
        }

        ++it;
    }

    if (!base) {
        return LINGLONG_ERR("couldn't find base layer");
    }

    // export layers
    auto layersDir = QDir{ bundleDir.absoluteFilePath("layers") };
    if (!layersDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(layersDir.absolutePath()));
    }

    QFile srcLoader;
    QString appID;
    for (const auto &layer : std::as_const(this->layers)) {
        auto infoRet = layer.info();
        if (!infoRet) {
            return LINGLONG_ERR(QString{ "failed export layer %1:" }.arg(layer.absolutePath()),
                                infoRet.error());
        }

        auto info = *infoRet;
        auto moduleDir =
          QDir{ layersDir.absoluteFilePath(QString::fromStdString(info.id) % QDir::separator()
                                           % QString::fromStdString(info.packageInfoV2Module)) };
        if (!moduleDir.mkpath(".")) {
            return LINGLONG_ERR(
              QString{ "couldn't create directory %1" }.arg(moduleDir.absolutePath()));
        }

        auto ret = filteringFiles(layer);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        const auto &[minified, files] = *ret;

        // first step, copy files which in layer directory
        std::error_code ec;
        const auto &infoList =
          layer.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        for (const auto &info : infoList) {
            const auto &componentName = info.fileName();
            // we will apply some filters to files later, skip
            if (componentName == "files") {
                continue;
            }

            std::filesystem::copy(info.absoluteFilePath().toStdString(),
                                  moduleDir.absoluteFilePath(componentName).toStdString(),
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::recursive,
                                  ec);
            if (ec) {
                return LINGLONG_ERR("couldn't copy from " % info.absoluteFilePath() % " to "
                                    % moduleDir.absoluteFilePath(componentName) % " "
                                    % QString::fromStdString(ec.message()));
            }
        };

        // second step, copy files which has been filtered
        auto basePath = std::filesystem::path{ layer.filesDirPath().toStdString() };
        if (!basePath.has_filename()) {
            return LINGLONG_ERR("the name of files directory is empty");
        }

        auto moduleFilesDir = moduleDir.absolutePath().toStdString() / basePath.filename();
        if (!std::filesystem::create_directories(moduleFilesDir, ec) && ec) {
            return LINGLONG_ERR(QString{ "couldn't create directory: %1, error: %2" }.arg(
              QString::fromStdString(moduleFilesDir.string()),
              QString::fromStdString(ec.message())));
        }

        auto symlinkCount = sysconf(_SC_SYMLOOP_MAX);
        if (symlinkCount < 0) {
            symlinkCount = 40;
        }

        if (!files.empty()) {
            struct statvfs moduleFilesDirStat{};
            struct statvfs filesStat{};

            if (statvfs(moduleFilesDir.c_str(), &moduleFilesDirStat) == -1) {
                return LINGLONG_ERR("couldn't stat module files directory: "
                                    + QString::fromStdString(moduleFilesDir));
            }

            if (statvfs((*files.begin()).c_str(), &filesStat) == -1) {
                return LINGLONG_ERR("couldn't stat files directory: "
                                    + QString::fromStdString(layer.filesDirPath().toStdString()));
            }

            const bool shouldCopy = moduleFilesDirStat.f_fsid != filesStat.f_fsid;
            for (std::filesystem::path source : files) {
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

                    return LINGLONG_ERR(QString{ "symlink_status error:%1" }.arg(
                      QString::fromStdString(ec.message())));
                }

                if (std::filesystem::is_directory(realSource)) {
                    std::filesystem::create_directories(realDestination, ec);
                    if (ec) {
                        return LINGLONG_ERR(QString{ "create_directories error:%1" }.arg(
                          QString::fromStdString(ec.message())));
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
                    return LINGLONG_ERR(QString{ "get symlink status of %1 failed: %2" }.arg(
                      QString::fromStdString(realDestination.string()),
                      QString::fromStdString(ec.message())));
                }

                if (!std::filesystem::exists(realDestination.parent_path(), ec)
                    && !std::filesystem::create_directories(realDestination.parent_path(), ec)
                    && ec) {
                    return LINGLONG_ERR(
                      "couldn't create directories "
                      % QString::fromStdString(realDestination.parent_path().string() + ":"
                                               + ec.message()));
                }

                if (shouldCopy) {
                    std::filesystem::copy(realSource, realDestination, ec);
                    if (ec) {
                        return LINGLONG_ERR("couldn't copy from " % QString::fromStdString(source)
                                            % " to "
                                            % QString::fromStdString(realDestination.string()) % " "
                                            % QString::fromStdString(ec.message()));
                    }

                    continue;
                }

                std::filesystem::create_hard_link(realSource, realDestination, ec);
                if (ec) {
                    return LINGLONG_ERR("couldn't link from " % QString::fromStdString(realSource)
                                        % " to " % QString::fromStdString(realDestination.string())
                                        % " " % QString::fromStdString(ec.message()));
                }
            }
        }

        auto &layerInfoRef = this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = info, .minified = minified });

        // third step, update meta infomation
        if (info.kind == "app") {
            if (!this->loader.isEmpty()) {
                srcLoader.setFileName(this->loader);
            }

            appID = QString::fromStdString(info.id);
            auto hasMinifiedDeps = std::any_of(this->meta.layers.cbegin(),
                                               this->meta.layers.cend(),
                                               [](const api::types::v1::UabLayer &layer) {
                                                   return layer.minified;
                                               });
            // app layer is the last layer, so we could update it's packageInfo directly
            auto appInfoPath = moduleDir.absoluteFilePath("info.json");
            if (hasMinifiedDeps) {
                info.uuid = this->meta.uuid;
            }

            std::ofstream stream;
            stream.open(appInfoPath.toStdString(), std::ios_base::out | std::ios_base::trunc);
            if (!stream.is_open()) {
                return LINGLONG_ERR("couldn't open file: " + appInfoPath);
            }
            stream << nlohmann::json(info).dump();
            layerInfoRef.info = info;
            continue;
        }

        // in only-App mode, after copying runtime files, append needed files from base to runtime
        // now we only have three layers
        if (base) {
            const QDir filesDir = base->absoluteFilePath("files");
            if (!filesDir.exists()) {
                return LINGLONG_ERR(
                  QString{ "files directory %1 doesn't exist" }.arg(filesDir.absolutePath()));
            }

            auto curArch = Architecture::currentCPUArchitecture();
            if (!curArch) {
                return LINGLONG_ERR("couldn't get current architecture");
            }
            const auto fakePrefix =
              moduleFilesDir / "lib" / std::filesystem::path{ curArch->getTriplet().toStdString() };

            for (std::filesystem::path file : this->neededFiles) {
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

                auto ret = prepareSymlink(filesDir.absolutePath().toStdString(),
                                          moduleFilesDir,
                                          file,
                                          symlinkCount);
                if (!ret) {
                    return LINGLONG_ERR(ret);
                }

                auto [source, destination] = std::move(ret).value();
                if (!std::filesystem::exists(source, ec)) {
                    if (ec) {
                        return LINGLONG_ERR(
                          QString{ "couldn't check file %1 exists: %2" }.arg(source.c_str(),
                                                                             ec.message().c_str()));
                    }

                    // source file must exist, it must be a regular file
                    return LINGLONG_ERR(QString{ "file %1 doesn't exist" }.arg(source.c_str()));
                }

                auto relative = destination.lexically_relative(fakePrefix);
                if (relative.empty() || relative.string().rfind("..", 0) == 0) {
                    // override the destination file
                    destination = fakePrefix / source.filename();
                }

                if (!std::filesystem::create_directories(destination.parent_path(), ec) && ec) {
                    return LINGLONG_ERR("couldn't create directories "
                                        % QString::fromStdString(destination.parent_path().string()
                                                                 + ":" + ec.message()));
                }

                std::filesystem::copy(source,
                                      destination,
                                      std::filesystem::copy_options::skip_existing,
                                      ec);
                if (ec) {
                    return LINGLONG_ERR(
                      QString{ "couldn't copy %1 to %2, error: %3" }.arg(source.c_str(),
                                                                         destination.c_str(),
                                                                         ec.message().c_str()));
                }
            }

            // update runtime info.json
            auto infoPath = moduleDir.absoluteFilePath("info.json");
            std::ofstream stream;
            stream.open(infoPath.toStdString(), std::ios_base::out | std::ios_base::trunc);
            if (!stream.is_open()) {
                return LINGLONG_ERR("couldn't open file: " + infoPath);
            }

            auto newSize = linglong::utils::calculateDirectorySize(moduleFilesDir);
            if (!newSize) {
                return LINGLONG_ERR(newSize);
            }
            info.size = static_cast<int64_t>(*newSize);

            stream << nlohmann::json(info).dump();
            layerInfoRef.info = info;
        }
    }

    if (srcLoader.fileName().isEmpty()) {
        // default loader
        auto uabLoader = !defaultLoader.isEmpty()
          ? defaultLoader
          : QDir{ LINGLONG_UAB_DATA_LOCATION }.absoluteFilePath("uab-loader");
        srcLoader.setFileName(uabLoader);
        if (!srcLoader.exists()) {
            return LINGLONG_ERR("the loader of uab application doesn't exist.");
        }
    }

    auto destLoader = QFile{ bundleDir.absoluteFilePath("loader") };
    if (!srcLoader.copy(destLoader.fileName())) {
        return LINGLONG_ERR(
          QString{ "couldn't copy loader %1 to %2: %3" }.arg(srcLoader.fileName(),
                                                             destLoader.fileName(),
                                                             srcLoader.errorString()));
    }

    if (!destLoader.setPermissions(destLoader.permissions() | QFile::ExeOwner | QFile::ExeGroup
                                   | QFile::ExeOther)) {
        return LINGLONG_ERR(destLoader);
    }

    // use custom loader doesn't need extra layer and ll-box any more
    if (!this->loader.isEmpty()) {
        return LINGLONG_OK;
    }

    // add extra data
    auto extraDir = QDir{ bundleDir.absoluteFilePath("extra") };
    if (!extraDir.mkpath(".")) {
        return LINGLONG_ERR(QString{ "couldn't create directory %1" }.arg(extraDir.absolutePath()));
    }

    // copy ll-box
    auto boxBin = !defaultBox.isEmpty() ? defaultBox : QStandardPaths::findExecutable("ll-box");
    if (boxBin.isEmpty()) {
        return LINGLONG_ERR("couldn't find ll-box");
    }
    auto srcBoxBin = QFile{ boxBin };
    auto destBoxBin = extraDir.filePath("ll-box");
    if (!srcBoxBin.copy(destBoxBin)) {
        return LINGLONG_ERR(
          QString{ "couldn't copy %1 to %2: %3" }.arg(boxBin, destBoxBin, srcBoxBin.errorString()));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::prepareDistributedBundle(const QDir &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare distributed bundle")

    // export layers
    auto layersDir = QDir{ bundleDir.absoluteFilePath("layers") };
    if (!layersDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(layersDir.absolutePath()));
    }

    // check if we can use hard links for optimization (only need to check once)
    struct statvfs layersDirStat{};
    if (statvfs(layersDir.absolutePath().toStdString().c_str(), &layersDirStat) == -1) {
        return LINGLONG_ERR("couldn't stat layers directory: " + layersDir.absolutePath());
    }

    for (const auto &layer : std::as_const(this->layers)) {
        auto infoRet = layer.info();
        if (!infoRet) {
            return LINGLONG_ERR(QString{ "failed export layer %1:" }.arg(layer.absolutePath()),
                                infoRet.error());
        }

        auto info = std::move(infoRet).value();
        LogI("info.id: {}, info.packageInfoV2Module: {}", info.id, info.packageInfoV2Module);
        auto moduleDir =
          QDir{ layersDir.absoluteFilePath(QString::fromStdString(info.id) % QDir::separator()
                                           % QString::fromStdString(info.packageInfoV2Module)) };
        if (!moduleDir.mkpath(".")) {
            return LINGLONG_ERR(
              QString{ "couldn't create directory %1" }.arg(moduleDir.absolutePath()));
        }

        // check if layer and target are on the same filesystem
        struct statvfs layerStat{};
        if (statvfs(layer.absolutePath().toStdString().c_str(), &layerStat) == -1) {
            return LINGLONG_ERR("couldn't stat layer directory: " + layer.absolutePath());
        }

        const bool shouldCopy = layerStat.f_fsid != layersDirStat.f_fsid;
        std::error_code ec;

        if (shouldCopy) {
            // different filesystem, need to copy files
            std::filesystem::copy(layer.absolutePath().toStdString(),
                                  moduleDir.absolutePath().toStdString(),
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::recursive,
                                  ec);
            if (ec) {
                return LINGLONG_ERR("couldn't copy from " % layer.absolutePath() % " to "
                                    % moduleDir.absolutePath() % " "
                                    % QString::fromStdString(ec.message()));
            }
        } else {
            // same filesystem, can use hard links for optimization
            // use recursive directory iterator to process all files
            std::error_code localEc;
            auto layerPath = std::filesystem::path(layer.absolutePath().toStdString());
            auto modulePath = std::filesystem::path(moduleDir.absolutePath().toStdString());

            // first, create the base directory
            if (!std::filesystem::create_directories(modulePath, localEc) && localEc) {
                return LINGLONG_ERR("couldn't create directory: "
                                    % QString::fromStdString(modulePath.string()) % " "
                                    % QString::fromStdString(localEc.message()));
            }

            // iterate through all files and directories recursively
            for (const auto &entry :
                 std::filesystem::recursive_directory_iterator(layerPath, localEc)) {
                if (localEc) {
                    return LINGLONG_ERR("couldn't iterate directory: "
                                        % QString::fromStdString(layerPath.string()) % " "
                                        % QString::fromStdString(localEc.message()));
                }

                auto relativePath = entry.path().lexically_relative(layerPath);
                auto destPath = modulePath / relativePath;

                if (entry.is_directory()) {
                    // create directory if it doesn't exist
                    std::filesystem::create_directories(destPath, localEc);
                    if (localEc) {
                        return LINGLONG_ERR("couldn't create directory: "
                                            % QString::fromStdString(destPath.string()) % " "
                                            % QString::fromStdString(localEc.message()));
                    }
                    continue;
                }

                // for non-directory files, create parent directories first
                std::filesystem::create_directories(destPath.parent_path(), localEc);
                if (localEc) {
                    return LINGLONG_ERR("couldn't create directories: "
                                        % QString::fromStdString(destPath.parent_path().string())
                                        % " " % QString::fromStdString(localEc.message()));
                }

                if (entry.is_symlink()) {
                    // handle symlinks - copy them directly to preserve the link target
                    std::filesystem::copy(entry.path(),
                                          destPath,
                                          std::filesystem::copy_options::copy_symlinks,
                                          localEc);
                    if (localEc) {
                        return LINGLONG_ERR("couldn't copy symlink from "
                                            % QString::fromStdString(entry.path().string()) % " to "
                                            % QString::fromStdString(destPath.string()) % " "
                                            % QString::fromStdString(localEc.message()));
                    }
                    continue;
                }

                // regular file - try to create hard link, fallback to copy if failed
                std::filesystem::create_hard_link(entry.path(), destPath, localEc);
                if (localEc) {
                    // fallback to copy if hard link fails
                    std::filesystem::copy(entry.path(),
                                          destPath,
                                          std::filesystem::copy_options::copy_symlinks,
                                          localEc);
                    if (localEc) {
                        return LINGLONG_ERR("couldn't copy from "
                                            % QString::fromStdString(entry.path().string()) % " to "
                                            % QString::fromStdString(destPath.string()) % " "
                                            % QString::fromStdString(localEc.message()));
                    }
                }
            }
        }

        // add layer info to meta
        this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = info, .minified = false });
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packBundle(bool distributedOnly) noexcept
{
    LINGLONG_TRACE("add layers to uab")

    auto bundleDir = QDir{ this->uab.parentDir().absoluteFilePath("bundle") };
    if (!bundleDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(bundleDir.absolutePath()));
    }

    auto bundleFile = this->uab.parentDir().absoluteFilePath("bundle.ef");
    if (QFile::exists(bundleFile) && !QFile::remove(bundleFile)) {
        return LINGLONG_ERR(QString{ "couldn't remove file %1" }.arg(bundleFile));
    }

    auto ret =
      distributedOnly ? prepareDistributedBundle(bundleDir) : prepareExecutableBundle(bundleDir);
    if (!ret) {
        return ret;
    }

    if (bundleCB) {
        ret = bundleCB(bundleFile, bundleDir.absolutePath());
        if (!ret) {
            return LINGLONG_ERR("bundle error", ret);
        }
    } else {
        // https://github.com/erofs/erofs-utils/blob/b526c0d7da46b14f1328594cf1d1b2401770f59b/README#L171-L183
        if (auto ret = utils::command::Cmd("mkfs.erofs")
                         .exec({ "-z" + compressor,
                                 "-Efragments,dedupe,ztailpacking",
                                 "-C1048576",
                                 "-b4096", // force 4096 block size, default is page size
                                 bundleFile,
                                 bundleDir.absolutePath() });
            !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    // calculate digest
    QFile bundle{ bundleFile };
    if (!bundle.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        return LINGLONG_ERR(bundle);
    }

    QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
    if (!cryptor.addData(&bundle)) {
        return LINGLONG_ERR(QString{ "failed to calculate digest from %1: %2" }.arg(bundleFile));
    }
    this->meta.digest = cryptor.result().toHex().toStdString();
    const auto *bundleSection = "linglong.bundle";
    if (auto ret = this->uab.addNewSection(bundleSection, QFileInfo{ bundleFile }); !ret) {
        return LINGLONG_ERR(ret);
    }
    this->meta.sections.bundle = bundleSection;

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packMetaInfo() noexcept
{
    LINGLONG_TRACE("add metaInfo to uab")

    auto metaFile = QFile{ this->uab.parentDir().absoluteFilePath("metaInfo.json") };
    if (!metaFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return LINGLONG_ERR(metaFile);
    }

    nlohmann::json metaInfo;
    api::types::v1::to_json(metaInfo, meta);
    if (metaFile.write(QByteArray::fromStdString(metaInfo.dump())) == -1) {
        return LINGLONG_ERR(metaFile);
    }
    metaFile.close();

    const auto *metaSection = "linglong.meta";
    if (auto ret = this->uab.addNewSection(metaSection,
                                           QFileInfo{ metaFile },
                                           { "linglong.meta=readonly,code" });
        !ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<std::pair<bool, std::unordered_set<std::string>>>
UABPackager::filteringFiles(const LayerDir &layer) const noexcept
{
    LINGLONG_TRACE("filtering files in layer directory")

    auto filesDir = layer.filesDirPath();
    if (!QFileInfo::exists(filesDir)) {
        return LINGLONG_ERR(
          QString{ "there isn't a files dir in layer %1" }.arg(layer.absolutePath()));
    }

    auto expandPaths = [prefix = std::filesystem::path{ filesDir.toStdString() }](
                         const std::unordered_set<std::string> &originalFiles)
      -> utils::error::Result<std::unordered_set<std::string>> {
        LINGLONG_TRACE("expand all filters")
        std::unordered_set<std::string> expandFiles;
        std::error_code ec;

        for (const auto &originalEntry : originalFiles) {
            auto entry = prefix / originalEntry.substr(1);
            auto status = std::filesystem::symlink_status(entry, ec);
            if (ec) {
                return LINGLONG_ERR(QString::fromStdString(ec.message()));
            }

            if (!std::filesystem::exists(status)) {
                if (ec) {
                    return LINGLONG_ERR(QString::fromStdString(ec.message()));
                }
                continue;
            }

            if (status.type() == std::filesystem::file_type::regular) {
                expandFiles.insert(entry);
                continue;
            }
            if (ec) {
                return LINGLONG_ERR(QString::fromStdString(ec.message()));
            }

            if (status.type() == std::filesystem::file_type::directory) {
                auto iterator = std::filesystem::recursive_directory_iterator(entry, ec);
                if (ec) {
                    return LINGLONG_ERR(QString::fromStdString(ec.message()));
                }

                for (const auto &file : iterator) {
                    if (file.is_directory(ec)) {
                        continue;
                    }

                    if (ec) {
                        return LINGLONG_ERR(QString::fromStdString(ec.message()));
                    }

                    expandFiles.insert(file.path().string());
                }
            }
            if (ec) {
                return LINGLONG_ERR(QString::fromStdString(ec.message()));
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

    std::error_code ec;
    auto iterator = std::filesystem::recursive_directory_iterator(filesDir.toStdString(), ec);
    if (ec) {
        return LINGLONG_ERR(QString::fromStdString(ec.message()));
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
              QString{ "failed to check file %1 type: %2" }.arg(file.path().c_str(),
                                                                ec.message().c_str()));
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
            return LINGLONG_ERR(QString{ "failed to load blacklist: %1" }.arg(ec.message().data()));
        }

        return LINGLONG_ERR(QString{ "backlist %1 doesn't exist" }.arg(blackListFile.c_str()));
    }

    std::ifstream stream{ blackListFile };
    if (!stream.is_open()) {
        return LINGLONG_ERR(QString{ "couldn't open blacklist: %1" }.arg(blackListFile.c_str()));
    }

    std::string line;
    while (!stream.eof()) {
        std::getline(stream, line);
        if (line.empty() || line.rfind('#', 0) == 0) {
            continue;
        }

        auto [_, success] = this->blackList.emplace(line);
        if (!success) {
            qWarning() << QString{ "duplicate entry: %1" }.arg(line.c_str());
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
          QString{ "%1 is generated by ldd-check but not found currently, it may be "
                   "deleted or ldd-check has been skipped" }
            .arg(file.c_str()));
    }

    auto content = YAML::LoadFile(file.string());
    auto node = content["depends"];
    if (!node || !node.IsSequence()) {
        return LINGLONG_ERR(QString{ "the content of %1 is invalid" }.arg(file.c_str()));
    }

    auto libs = node.as<std::vector<std::string>>();
    for (const auto &libStr : libs) {
        std::filesystem::path lib{ libStr };

        if (lib.empty()) {
            continue;
        }

        if (!lib.is_absolute()) {
            return LINGLONG_ERR(QString{ "invalid format, lib: %1" }.arg(lib.c_str()));
        }

        auto [_, success] = this->neededFiles.insert(lib.lexically_relative("/"));
        if (!success) {
            return LINGLONG_ERR(QString{ "duplicate entry: %1" }.arg(lib.c_str()));
        }
    }

    return LINGLONG_OK;
}

void UABPackager::setLoader(const QString &loader) noexcept
{
    this->loader = loader;
}

void UABPackager::setCompressor(const QString &compressor) noexcept
{
    this->compressor = compressor;
}

void UABPackager::setDefaultHeader(const QString &header) noexcept
{
    this->defaultHeader = header;
}

void UABPackager::setDefaultLoader(const QString &loader) noexcept
{
    this->defaultLoader = loader;
}

void UABPackager::setDefaultBox(const QString &box) noexcept
{
    this->defaultBox = box;
}

void UABPackager::setBundleCB(
  std::function<utils::error::Result<void>(const QString &, const QString &)> bundleCB) noexcept
{
    this->bundleCB = std::move(bundleCB);
}

} // namespace linglong::package
