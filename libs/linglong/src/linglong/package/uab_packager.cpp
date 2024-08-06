// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"

#include <yaml-cpp/yaml.h>

#include <QCryptographicHash>
#include <QStandardPaths>

#include <filesystem>
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
                                                    const QFileInfo &dataFile) const noexcept
{
    LINGLONG_TRACE(QString{ "add section:%1" }.arg(QString{ sectionName }))

    auto ret = utils::command::Exec(
      "objcopy",
      { QString{ "--add-section" },
        QString{ "%1=%2" }.arg(QString{ sectionName }).arg(dataFile.absoluteFilePath()),
        this->elfPath(),
        this->elfPath() });
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

utils::error::Result<void> UABPackager::applyYamlFilter(const QFileInfo &filter) noexcept
{
    LINGLONG_TRACE("apply uab minified filters")

    if (!filter.isFile() || filter.suffix() != "yaml") {
        return LINGLONG_ERR("filter isn't a yaml file");
    }

    YAML::Node content;
    try {
        content = YAML::LoadFile(filter.absoluteFilePath().toStdString());
    } catch (YAML::ParserException &e) {
        return LINGLONG_ERR(e.what());
    } catch (YAML::BadFile &e) {
        return LINGLONG_ERR(e.what());
    }

    auto extraLibs = content["extra_libs"];
    if (!extraLibs.IsDefined() || !extraLibs.IsSequence()) {
        return LINGLONG_ERR(
          QString{ "not found filter sequence in %1" }.arg(filter.absoluteFilePath()));
    }

    for (const auto &val : extraLibs) {
        auto filter = val.as<QString>(QString{});
        if (filter.isEmpty()) {
            continue;
        }

        if (filter.startsWith(QDir::separator())) {
            filter.remove(0, 1);
        }

        filterSet.emplace(filter.toStdString());
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
    if (auto ret = this->uab.addNewSection(iconSection, iconAchieve); !ret) {
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
        for (const auto &info : layer.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            const auto &componentName = info.fileName();

            std::filesystem::copy(info.absoluteFilePath().toStdString(),
                                  moduleDir.absoluteFilePath(componentName).toStdString(),
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::update_existing,
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
        if (!std::filesystem::create_directory(moduleFilesDir, ec) && ec) {
            return LINGLONG_ERR(QString{ "couldn't create directory: %1, error: %2" }
                                  .arg(QString::fromStdString(moduleFilesDir.string()))
                                  .arg(QString::fromStdString(ec.message())));
        }

        for (const auto &source : files) {
            auto destination = moduleFilesDir / source.lexically_relative(basePath);

            if (std::filesystem::is_symlink(source, ec)) {
                std::filesystem::copy_symlink(source, destination, ec);
                if (ec) {
                    return LINGLONG_ERR("couldn't copy symlink from "
                                        % QString::fromStdString(source.string()) % " to "
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
                if (!std::filesystem::create_directory(destination, ec) && ec) {
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

            std::filesystem::copy_file(source,
                                       destination,
                                       std::filesystem::copy_options::update_existing,
                                       ec);
            if (ec) {
                return LINGLONG_ERR("couldn't copy from " % QString::fromStdString(source.string())
                                    % " to " % QString::fromStdString(destination.string()) % " "
                                    % QString::fromStdString(ec.message()));
            }
        }

        // third step, update meta infomation
        this->meta.layers.push_back({ .info = info, .minified = minified });
        if (info.kind == "app") {
            appID = QString::fromStdString(info.id);
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
    auto arch = Architecture::parse(QSysInfo::currentCpuArchitecture());
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
    if (auto ret = this->uab.addNewSection(bundleSection, bundleFile); !ret) {
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
    if (auto ret = this->uab.addNewSection(metaSection, metaFile); !ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<std::pair<bool, std::vector<std::filesystem::path>>>
UABPackager::filteringFiles(const LayerDir &layer) noexcept
{
    LINGLONG_TRACE("filtering files in layer directory")

    auto filesDir = layer.filesDirPath();
    if (!QFileInfo::exists(filesDir)) {
        return LINGLONG_ERR(
          QString{ "there isn't a files dir in layer %1" }.arg(layer.absolutePath()));
    }
    auto prefix = std::filesystem::path{ filesDir.toStdString() };

    std::error_code ec;
    auto iterator = std::filesystem::recursive_directory_iterator(filesDir.toStdString(), ec);
    if (ec) {
        return LINGLONG_ERR(QString::fromStdString(ec.message()));
    }

    std::vector<std::filesystem::path> files;
    bool minified{ false };
    for (const auto &entry : iterator) {
        auto path = entry.path();
        if (filterSet.count(path.lexically_relative(prefix).string()) == 1) {
            minified = true;
            qDebug() << "file" << QString::fromStdString(path.string()) << "has been filtered";
            continue;
        }

        files.emplace_back(std::move(path));
    }

    std::sort(files.begin(), files.end());
    return std::make_pair(minified, std::move(files));
}

} // namespace linglong::package

// using QString fully-specialized class template only affects the current compilation unit.
// maybe we could provide a Qt wrapper for yaml-cpp
namespace YAML {
template<>
struct convert<QString>
{
    static Node encode(const QString &rhs) { return Node(rhs.toStdString()); }

    static bool decode(const Node &node, QString &rhs)
    {
        if (!node.IsScalar()) {
            return false;
        }

        rhs = QString::fromStdString(node.Scalar());
        return true;
    }
};
} // namespace YAML
