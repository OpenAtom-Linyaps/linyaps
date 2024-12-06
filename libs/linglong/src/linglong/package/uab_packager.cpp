// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/serialize/json.h"

#include <yaml-cpp/yaml.h>

#include <QCryptographicHash>
#include <QStandardPaths>

#include <fstream>
#include <unordered_set>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
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

utils::error::Result<void> UABPackager::pack(const QString &uabFilename) noexcept
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

    if (auto ret = packBundle(); !ret) {
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

utils::error::Result<void> UABPackager::prepareBundle(const QDir &bundleDir) noexcept
{
    LINGLONG_TRACE("prepare layers for make a bundle")

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

        const auto &info = *infoRet;
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

            std::filesystem::create_hard_link(source, destination, ec);
            if (ec) {
                return LINGLONG_ERR("couldn't link from " % QString::fromStdString(source) % " to "
                                    % QString::fromStdString(destination.string()) % " "
                                    % QString::fromStdString(ec.message()));
            }
        }

        // third step, update meta infomation
        this->meta.layers.push_back({ .info = info, .minified = minified });
        if (info.kind == "app") {
            appID = QString::fromStdString(info.id);
            auto hasMinifiedDeps = std::any_of(this->meta.layers.cbegin(),
                                               this->meta.layers.cend(),
                                               [](const api::types::v1::UabLayer &layer) {
                                                   return layer.minified;
                                               });
            if (!hasMinifiedDeps) {
                break;
            }

            // app layer is the last layer, so we could update it's packageInfo directly
            auto appInfoPath = moduleDir.absoluteFilePath("info.json");
            auto info =
              linglong::utils::serialize::LoadJSONFile<linglong::api::types::v1::PackageInfoV2>(
                appInfoPath);
            if (!ret) {
                return LINGLONG_ERR(info);
            }
            auto newInfo = *info;
            newInfo.uuid = this->meta.uuid;

            std::ofstream stream;
            stream.open(appInfoPath.toStdString(), std::ios_base::out | std::ios_base::trunc);
            if (!stream.is_open()) {
                return LINGLONG_ERR("couldn't open file: " + appInfoPath);
            }
            stream << nlohmann::json(newInfo).dump();
        }
    }

    // add extra data
    auto extraDir = QDir{ bundleDir.absoluteFilePath("extra") };
    if (!extraDir.mkpath(".")) {
        return LINGLONG_ERR(QString{ "couldn't create directory %1" }.arg(extraDir.absolutePath()));
    }

    // copy config.json and generators
    auto srcCfgDir = QDir{ LINGLONG_INSTALL_PREFIX "/lib/linglong/container" };
    if (!srcCfgDir.exists()) {
        return LINGLONG_ERR(QString("the container configuration directory doesn't exist: %1")
                              .arg(srcCfgDir.absolutePath()));
    }

    auto destCfgDir = QDir{ extraDir.absoluteFilePath("container") };
    if (!destCfgDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(destCfgDir.absolutePath()));
    };

    auto srcInitCfg = QFile{ srcCfgDir.absoluteFilePath("config.json") };
    if (!srcInitCfg.exists() || !QFileInfo{ srcInitCfg }.isFile()) {
        return LINGLONG_ERR(
          QString{ "%1 doesn't exist or it's not a file" }.arg(srcInitCfg.fileName()));
    }
    auto destInitCfg = destCfgDir.absoluteFilePath("config.json");
    if (!srcInitCfg.copy(destInitCfg)) {
        return LINGLONG_ERR(QString{ "couldn't copy %1 to %2: %3" }
                              .arg(srcInitCfg.fileName())
                              .arg(destInitCfg)
                              .arg(srcInitCfg.errorString()));
    }

    auto srcGens = QDir{ srcCfgDir.absoluteFilePath("config.d") };
    if (!srcGens.exists() || !QFileInfo{ srcGens.absolutePath() }.isDir()) {
        return LINGLONG_ERR(
          QString{ "%1 doesn't exist or it's not a directory" }.arg(srcGens.absolutePath()));
    }
    auto destGens = QDir{ destCfgDir.absoluteFilePath("config.d") };
    if (!destGens.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(destCfgDir.absolutePath()));
    }
    for (const auto &gen : srcGens.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
        QFile realGen{ gen.absoluteFilePath() };
        if (gen.isExecutable() && gen.isSymLink()) {
            realGen.setFileName(gen.symLinkTarget());
        } else if (!gen.fileName().endsWith(".json")) {
            qWarning() << "unknown config generator" << gen.absoluteFilePath();
            continue;
        }

        if (!realGen.exists()) {
            return LINGLONG_ERR(
              QString{ "config generator %1 doesn't exist" }.arg(realGen.fileName()));
        }

        auto destGen = destGens.absoluteFilePath(QFileInfo{ realGen.fileName() }.fileName());
        if (!realGen.copy(destGen)) {
            return LINGLONG_ERR(QString{ "couldn't copy %1 to %2: %3" }
                                  .arg(realGen.fileName())
                                  .arg(destGen)
                                  .arg(realGen.errorString()));
        }
    }

    // generate ld configs
    auto arch = Architecture::currentCPUArchitecture();
    auto ldConfsDir = QDir{ extraDir.absoluteFilePath("ld.conf.d") };
    if (!ldConfsDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(destCfgDir.absolutePath()));
    }

    auto ldConf = QFile{ ldConfsDir.absoluteFilePath("zz_deepin-linglong-app.ld.so.conf") };
    if (!ldConf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return LINGLONG_ERR(ldConf);
    }

    QTextStream stream{ &ldConf };
    stream << "/runtime/lib" << Qt::endl;
    stream << "/runtime/lib/" + arch->getTriplet() << Qt::endl;
    stream << "/opt/apps/" + appID + "/files/lib" << Qt::endl;
    stream << "/opt/apps/" + appID + "/files/lib/" + arch->getTriplet() << Qt::endl;
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

utils::error::Result<void> UABPackager::packBundle() noexcept
{
    LINGLONG_TRACE("add layers to uab")

    auto bundleDir = QDir{ this->uab.parentDir().absoluteFilePath("bundle") };
    if (!bundleDir.mkpath(".")) {
        return LINGLONG_ERR(
          QString{ "couldn't create directory %1" }.arg(bundleDir.absolutePath()));
    }

    auto bundleFile = this->uab.parentDir().absoluteFilePath("bundle.ef");
    if (!QFileInfo::exists(bundleFile)) {
        auto ret = prepareBundle(bundleDir);
        if (!ret) {
            return ret;
        }

        if (auto ret =
              utils::command::Exec("mkfs.erofs",
                                   { "-zlz4hc,9", "-b4096", bundleFile, bundleDir.absolutePath() });
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

    std::unordered_set<std::string> allFiles;
    for (const auto &file : iterator) {
        allFiles.insert(file.path().string());
    }

    bool minified = !expandedExcludes.empty();
    for (const auto &excludeFile : expandedExcludes) {
        allFiles.erase(excludeFile);
    }

    return std::make_pair(minified, std::move(allFiles));
}

} // namespace linglong::package
