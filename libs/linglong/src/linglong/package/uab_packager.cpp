// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/Version.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"

#include <QCryptographicHash>
#include <QStandardPaths>

#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <filesystem>

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

utils::error::Result<void> UABPackager::setIcon(const QFileInfo &newIcon)
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

utils::error::Result<void> UABPackager::appendLayer(const LayerDir &layer)
{
    LINGLONG_TRACE("append layer to uab")

    if (!layer.exists()) {
        return LINGLONG_ERR("icon doesn't exists");
    }

    layers.append(layer);
    return LINGLONG_OK;
}

utils::error::Result<void> UABPackager::pack(const QString &uabFilename)
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
        auto layerDir =
          QDir{ layersDir.absoluteFilePath(QString::fromStdString(info.id) % QDir::separator()
                                           % QString::fromStdString(info.packageInfoV2Module)) };
        if (!layerDir.mkpath(".")) {
            return LINGLONG_ERR(
              QString{ "couldn't create directory %1" }.arg(layerDir.absolutePath()));
        }

        // copy all files currently
        for (const auto &info :
             layer.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
            if (info.fileName().startsWith("minified")) {
                continue;
            }

            std::error_code ec;
            std::filesystem::copy(info.absolutePath().toStdString(),
                                  layerDir.absolutePath().toStdString(),
                                  std::filesystem::copy_options::copy_symlinks
                                    | std::filesystem::copy_options::recursive
                                    | std::filesystem::copy_options::update_existing,
                                  ec);
            if (ec) {
                return LINGLONG_ERR(QString::fromStdString(ec.message()));
            }
        };

        this->meta.layers.push_back({ .info = info, .minified = false });
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
        if (gen.isExecutable()) {
            realGen.setFileName(LINGLONG_LIBEXEC_DIR % QDir::separator() + gen.fileName());
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

        if (auto ret = utils::command::Exec("mkfs.erofs", { bundleFile, bundleDir.absolutePath() });
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

} // namespace linglong::package
