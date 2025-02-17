// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/serialize/json.h"

#include <qglobal.h>
#include <yaml-cpp/yaml.h>

#include <QCryptographicHash>
#include <QStandardPaths>

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
                                                    QStringList flags) const noexcept
{
    LINGLONG_TRACE(QString{ "add section:%1" }.arg(QString{ sectionName }))

    auto args = QStringList{
        QString{ "--add-section" },
        QString{ "%1=%2" }.arg(QString{ sectionName }).arg(dataFile.absoluteFilePath())
    };

    if (!flags.empty()) {
        args.append({ "--set-section-flags", flags.join(QString{ "," }) });
    }

    args.append({ this->elfPath(), this->elfPath() });
    auto ret = utils::command::Exec("objcopy", args);
    if (!ret) {
        return LINGLONG_ERR(ret.error());
    }

    return LINGLONG_OK;
}

UABPackager::UABPackager(const QDir &workingDir)
{
    auto buildDir = QDir{ workingDir.absoluteFilePath(".uabBuild") };
    if (!buildDir.mkpath(".")) {
        qFatal("working directory of uab packager doesn't exists.");
    }

    this->buildDir = std::move(buildDir);
    this->workDir = workingDir.absolutePath().toStdString();

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

utils::error::Result<void> UABPackager::pack(const QString &uabFilename, bool onlyApp) noexcept
{
    LINGLONG_TRACE("package uab")

    auto uabHeader = QDir{ LINGLONG_UAB_DATA_LOCATION }.filePath("uab-header");
    if (!QFileInfo::exists(uabHeader)) {
        return LINGLONG_ERR("uab-header is missing");
    }

    auto uabApp = buildDir.filePath(uabFilename);
    if (QFileInfo::exists(uabApp) && !QFile::remove(uabApp)) {
        return LINGLONG_ERR("couldn't remove uab cache");
    }

    if (!QFile::copy(uabHeader, uabApp)) {
        return LINGLONG_ERR(
          QString{ "couldn't copy uab header from %1 to %2" }.arg(uabHeader).arg(uabApp));
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

    if (auto ret = packBundle(onlyApp); !ret) {
        return ret;
    }

    if (auto ret = packMetaInfo(); !ret) {
        return ret;
    }

    auto exportPath =
      QFileInfo{ this->buildDir.absolutePath() }.dir().absoluteFilePath(uabFilename);

    if (QFileInfo::exists(exportPath) && !QFile::remove(exportPath)) {
        return LINGLONG_ERR("couldn't remove previous uab file");
    }

    if (QFileInfo::exists(exportPath) && QFile::remove(exportPath)) {
        return LINGLONG_ERR(
          QString{ "file %1 already exist and could't remove it" }.arg(exportPath));
    }

    if (!QFile::copy(this->uab.elfPath(), exportPath)) {
        return LINGLONG_ERR(QString{ "export uab from %1 to %2 failed" }
                              .arg(QString{ this->uab.elfPath() })
                              .arg(exportPath));
    }

    if (!QFile::setPermissions(exportPath,
                               QFile::permissions(exportPath) | QFile::ExeOwner | QFile::ExeGroup
                                 | QFile::ExeOther)) {
        return LINGLONG_ERR("couldn't set executable permission to uab");
    }

    if (!QFile::remove(this->uab.elfPath())) {
        qWarning() << "couldn't remove" << this->uab.elfPath() << ", please remove it manually";
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packIcon() noexcept
{
    LINGLONG_TRACE("add icon to uab")

    auto iconAchieve = this->uab.parentDir().absoluteFilePath("icon.a");
    if (auto ret = utils::command::Exec("ar", { "q", iconAchieve, icon->absoluteFilePath() });
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

utils::error::Result<void> UABPackager::prepareBundle(const QDir &bundleDir, bool onlyApp) noexcept
{
    LINGLONG_TRACE("prepare layers for make a bundle")

    std::optional<LayerDir> base;
    if (onlyApp) {
        this->meta.onlyApp = true;
        for (auto it = this->layers.begin(); it != this->layers.end(); ++it) {
            auto infoRet = it->info();
            if (!infoRet) {
                return LINGLONG_ERR(QString{ "failed export layer %1:" }.arg(it->absolutePath()),
                                    infoRet.error());
            }

            const auto &info = *infoRet;
            if (info.id == "org.deepin.base") {
                base = *it;
                this->layers.erase(it);
                break;
            }
        }

        if (!base) {
            return LINGLONG_ERR("couldn't find base layer");
        }
    }

    auto uabDataDir = QDir{ LINGLONG_UAB_DATA_LOCATION };
    // copy loader
    auto srcLoader = QFile{ uabDataDir.absoluteFilePath("uab-loader") };
    if (!srcLoader.exists()) {
        return LINGLONG_ERR("the loader of uab application doesn't exist.");
    }

    auto destLoader = QFile{ bundleDir.absoluteFilePath("loader") };
    if (!srcLoader.copy(destLoader.fileName())) {
        return LINGLONG_ERR(QString{ "couldn't copy loader %1 to %2: %3" }
                              .arg(srcLoader.fileName())
                              .arg(destLoader.fileName())
                              .arg(srcLoader.errorString()));
    }

    if (!destLoader.setPermissions(destLoader.permissions() | QFile::ExeOwner | QFile::ExeGroup
                                   | QFile::ExeOther)) {
        return LINGLONG_ERR(destLoader);
    }

    // export layers
    auto layersDir = QDir{ bundleDir.absoluteFilePath("layers") };
    if (!layersDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(layersDir.absolutePath()));
    }

    QString appID;
    for (const auto &layer : this->layers) {
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

        if (files.empty()) {
            continue;
        }

        // first step, copy files which in layer directory
        std::error_code ec;
        for (const auto &info :
             layer.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
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
            return LINGLONG_ERR(QString{ "couldn't create directory: %1, error: %2" }
                                  .arg(QString::fromStdString(moduleFilesDir.string()))
                                  .arg(QString::fromStdString(ec.message())));
        }

        struct stat moduleFilesDirStat{}, filesStat{};
        if (stat(moduleFilesDir.c_str(), &moduleFilesDirStat) == -1) {
            return LINGLONG_ERR("couldn't stat module files directory: "
                                + QString::fromStdString(moduleFilesDir));
        }

        if (stat((*files.begin()).c_str(), &filesStat) == -1) {
            return LINGLONG_ERR("couldn't stat files directory: "
                                + QString::fromStdString(layer.filesDirPath().toStdString()));
        }

        const bool shouldCopy = moduleFilesDirStat.st_dev != filesStat.st_dev;

        for (const auto &source : files) {
            auto destination =
              moduleFilesDir / std::filesystem::path{ source }.lexically_relative(basePath);

            // Ensure that the parent directory exists
            if (!std::filesystem::create_directories(destination.parent_path(), ec) && ec) {
                return LINGLONG_ERR("couldn't create directories "
                                    % QString::fromStdString(destination.parent_path().string()
                                                             + ":" + ec.message()));
            }

            if (std::filesystem::is_symlink(source, ec)) {
                std::filesystem::copy_symlink(source, destination, ec);
                if (ec) {
                    return LINGLONG_ERR("couldn't copy symlink from "
                                        % QString::fromStdString(source) % " to "
                                        % QString::fromStdString(destination.string()) % " "
                                        % QString::fromStdString(ec.message()));
                }

                continue;
            }
            if (ec) {
                return LINGLONG_ERR(
                  QString{ "is_symlink error:%1" }.arg(QString::fromStdString(ec.message())));
            }

            if (std::filesystem::is_directory(source, ec)) {
                if (!std::filesystem::create_directories(destination, ec) && ec) {
                    return LINGLONG_ERR(QString{ "couldn't create directory: %1, error: %2" }
                                          .arg(QString::fromStdString(destination.string()))
                                          .arg(QString::fromStdString(ec.message())));
                }

                continue;
            }
            if (ec) {
                return LINGLONG_ERR(
                  QString{ "is_directory error:%1" }.arg(QString::fromStdString(ec.message())));
            }

            if (shouldCopy) {
                std::filesystem::copy(source,
                                      destination,
                                      std::filesystem::copy_options::overwrite_existing,
                                      ec);
                if (ec) {
                    return LINGLONG_ERR("couldn't copy from " % QString::fromStdString(source)
                                        % " to " % QString::fromStdString(destination.string())
                                        % " " % QString::fromStdString(ec.message()));
                }

                continue;
            }

            std::filesystem::create_hard_link(source, destination, ec);
            if (ec) {
                return LINGLONG_ERR("couldn't link from " % QString::fromStdString(source) % " to "
                                    % QString::fromStdString(destination.string()) % " "
                                    % QString::fromStdString(ec.message()));
            }
        }
        auto &layerInfoRef = this->meta.layers.emplace_back(
          linglong::api::types::v1::UabLayer{ .info = info, .minified = minified });

        // third step, update meta infomation
        if (info.kind == "app") {
            appID = QString::fromStdString(info.id);
            auto hasMinifiedDeps = std::any_of(this->meta.layers.cbegin(),
                                               this->meta.layers.cend(),
                                               [](const api::types::v1::UabLayer &layer) {
                                                   return layer.minified;
                                               });
            if (!hasMinifiedDeps && !onlyApp) {
                continue;
            }

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
            QDir filesDir = base->absoluteFilePath("files");
            if (!filesDir.exists()) {
                return LINGLONG_ERR(
                  QString{ "files directory %1 doesn't exist" }.arg(filesDir.absolutePath()));
            }

            for (std::filesystem::path file : this->neededFiles) {
                std::filesystem::path source =
                  filesDir.absoluteFilePath(QString::fromStdString(file)).toStdString();
                auto destination = moduleFilesDir / file;

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

                int resolveDepth{ 10 };
                while (resolveDepth > 0) {
                    // ensure parent directory exist.
                    auto parent = destination.parent_path();
                    if (!std::filesystem::create_directories(parent, ec) && ec) {
                        return LINGLONG_ERR(
                          QString{ "failed to create parent path of destination file: %1" }.arg(
                            parent.c_str()));
                    }

                    auto status = std::filesystem::symlink_status(source, ec);
                    if (ec) {
                        return LINGLONG_ERR("symlink_status error:"
                                            + QString::fromStdString(ec.message()));
                    }
                    if (status.type() != std::filesystem::file_type::symlink) {
                        break;
                    }

                    auto target = std::filesystem::read_symlink(source, ec);
                    if (ec) {
                        return LINGLONG_ERR("read_symlink error:"
                                            + QString::fromStdString(ec.message()));
                    }

                    std::filesystem::create_symlink(target, destination, ec);
                    if (ec && ec != std::errc::file_exists) {
                        return LINGLONG_ERR("couldn't create symlink from "
                                            % QString::fromStdString(source) % " to "
                                            % QString::fromStdString(destination.string()) % " "
                                            % QString::fromStdString(ec.message()));
                    }

                    if (target.is_relative()) {
                        target = std::filesystem::canonical(source.parent_path() / target)
                                   .lexically_relative(filesDir.absolutePath().toStdString());
                    }

                    source =
                      filesDir.absoluteFilePath(QString::fromStdString(target)).toStdString();
                    destination = moduleFilesDir / target;
                    --resolveDepth;
                }

                if (resolveDepth == 0) {
                    return LINGLONG_ERR(
                      QString{ "resolve symlink %1 too deep" }.arg(source.c_str()));
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
            info.size = static_cast<int>(*newSize);

            stream << nlohmann::json(info).dump();
            layerInfoRef.info = info;
        }
    }

    // add extra data
    auto extraDir = QDir{ bundleDir.absoluteFilePath("extra") };
    if (!extraDir.mkpath(".")) {
        return LINGLONG_ERR(QString{ "couldn't create directory %1" }.arg(extraDir.absolutePath()));
    }

    // generate ld configs
    auto arch = Architecture::currentCPUArchitecture();
    auto ldConfsDir = QDir{ extraDir.absoluteFilePath("ld.conf.d") };
    if (!ldConfsDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(ldConfsDir.absolutePath()));
    }

    auto ldConf = QFile{ ldConfsDir.absoluteFilePath("zz_deepin-linglong-app.ld.so.conf") };
    if (!ldConf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return LINGLONG_ERR(ldConf);
    }

    QTextStream stream{ &ldConf };
    stream << "/runtime/usr/lib/" << Qt::endl; // for only-app
    stream << "/runtime/lib/" << Qt::endl;
    stream << "/runtime/lib/" + arch->getTriplet() << Qt::endl;
    stream << "/opt/apps/" + appID + "/files/lib" << Qt::endl;
    stream << "/opt/apps/" + appID + "/files/lib/" + arch->getTriplet() << Qt::endl;
    if (onlyApp) {
        stream << "include /etc/ld.so.conf" << Qt::endl;
    }
    stream.flush();

    // copy ll-box
    auto boxBin = QStandardPaths::findExecutable("ll-box");
    if (boxBin.isEmpty()) {
        return LINGLONG_ERR("couldn't find ll-box");
    }
    auto srcBoxBin = QFile{ boxBin };
    auto destBoxBin = extraDir.filePath("ll-box");
    if (!srcBoxBin.copy(destBoxBin)) {
        return LINGLONG_ERR(QString{ "couldn't copy %1 to %2: %3" }
                              .arg(boxBin)
                              .arg(destBoxBin)
                              .arg(srcBoxBin.errorString()));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::packBundle(bool onlyApp) noexcept
{
    LINGLONG_TRACE("add layers to uab")

    auto bundleDir = QDir{ this->uab.parentDir().absoluteFilePath("bundle") };
    if (!bundleDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(bundleDir.absolutePath()));
    }

    auto bundleFile = this->uab.parentDir().absoluteFilePath("bundle.ef");
    if (!QFileInfo::exists(bundleFile)) {
        auto ret = prepareBundle(bundleDir, onlyApp);
        if (!ret) {
            return ret;
        }

        // https://github.com/erofs/erofs-utils/blob/b526c0d7da46b14f1328594cf1d1b2401770f59b/README#L171-L183
        if (auto ret =
              utils::command::Exec("mkfs.erofs",
                                   { "-zzstd,17",
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
            if (!std::filesystem::exists(entry, ec)) {
                if (ec) {
                    return LINGLONG_ERR(QString::fromStdString(ec.message()));
                }
                continue;
            }

            if (std::filesystem::is_regular_file(entry, ec)) {
                expandFiles.insert(entry);
                continue;
            }
            if (ec) {
                return LINGLONG_ERR(QString::fromStdString(ec.message()));
            }

            if (std::filesystem::is_directory(entry, ec)) {
                auto iterator = std::filesystem::recursive_directory_iterator(entry, ec);
                if (ec) {
                    return LINGLONG_ERR(QString::fromStdString(ec.message()));
                }

                for (const auto &file : iterator) {
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
    for (std::filesystem::path file : iterator) {
        auto fileName = file.filename().string();
        auto it =
          std::find_if(this->blackList.begin(),
                       this->blackList.end(),
                       [&fileName](const std::string &entry) {
                           return entry.rfind(fileName, 0) == 0 || fileName.rfind(entry, 0) == 0;
                       });
        auto status = std::filesystem::symlink_status(file, ec);
        if (ec) {
            return LINGLONG_ERR(
              QString{ "failed to get file %1 status %2" }.arg(file.c_str(), ec.message().c_str()));
        }

        if (it != this->blackList.end() && status.type() == std::filesystem::file_type::regular) {
            continue;
        }

        if (expandedExcludes.find(file) != expandedExcludes.cend()) {
            minified = true;
            continue;
        }

        allFiles.insert(file);
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
    for (std::filesystem::path lib : libs) {
        if (lib.empty() || !lib.is_absolute()) {
            return LINGLONG_ERR(QString{ "invalid format, lib: %1" }.arg(lib.c_str()));
        }

        auto [_, success] = this->neededFiles.insert(lib.lexically_relative("/"));
        if (!success) {
            return LINGLONG_ERR(QString{ "duplicate entry: %1" }.arg(lib.c_str()));
        }
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
