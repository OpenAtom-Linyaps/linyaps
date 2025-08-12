/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/utils/command/cmd.h"
#include "linglong/utils/command/env.h"

#include <qcontainerfwd.h>

#include <QDataStream>
#include <QSysInfo>

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace linglong::package {

LayerPackager::LayerPackager()
{
    this->initWorkDir();
}

utils::error::Result<void> LayerPackager::initWorkDir()
{
    LINGLONG_TRACE("init work dir");
    // initWorkDir仅在单元测试中多次调用，在正常使用中仅在构造时调用一次
    // 防御性编程，避免initWorkDir多次调用时，旧的workDir目录未被删除
    if (!this->workDir.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(this->workDir, ec);
        if (ec) {
            return LINGLONG_ERR("failed to remove work dir", ec);
        }
    }
    // 优先使用环境变量LINGLONG_TMPDIR指定的目录，默认为/var/tmp，避免/tmp是tmpfs内存不足
    auto dirName =
      "linglong-layer-workdir-" + QUuid::createUuid().toString(QUuid::Id128).toStdString();
    auto tmpDir = utils::command::getEnv("LINGLONG_TMPDIR");
    auto dirPath = std::filesystem::path(tmpDir.value_or("/var/tmp")) / dirName;
    auto ret = this->mkdirDir(dirPath);
    if (!ret.has_value()) {
        // 如果/var/tmp目录无权限创建，则使用临时目录
        dirPath = std::filesystem::temp_directory_path() / dirName;
        ret = this->mkdirDir(dirPath);
        if (!ret) {
            qCritical() << "failed to set work dir" << ret.error().message();
            Q_ASSERT(false);
        }
    }
    this->workDir = dirPath;
    return LINGLONG_OK;
}

const std::filesystem::path &LayerPackager::getWorkDir() const
{
    return this->workDir;
}

// 创建目录，用于单元测试
utils::error::Result<void> LayerPackager::mkdirDir(const std::string &path) noexcept
{
    LINGLONG_TRACE("mkdir dir" + path);
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create directory" + path, ec);
    }
    return LINGLONG_OK;
}

LayerPackager::~LayerPackager()
{
    if (this->isMounted) {
        auto ret = utils::command::Cmd("fusermount")
                     .exec({ "-z", "-u", (this->workDir / "unpack").string().c_str() });
        if (!ret) {
            qWarning() << "failed to umount " << (this->workDir / "unpack").c_str()
                       << ", please umount it manually";
        }
    }
    if (!std::filesystem::remove_all(this->workDir)) {
        qCritical() << "remove" << this->workDir.c_str() << "failed";
        Q_ASSERT(false);
    }
}

utils::error::Result<QSharedPointer<LayerFile>>
LayerPackager::pack(const LayerDir &dir, const QString &layerFilePath) const
{
    LINGLONG_TRACE("pack layer");

    QFile layer(layerFilePath);
    if (layer.exists()) {
        layer.remove();
    }

    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(magicNumber) < 0) {
        return LINGLONG_ERR(layer);
    }

    // generate LayerInfo
    api::types::v1::LayerInfo layerInfo;
    // layer info version not used yet, so give fixed value
    // keep it for later function expansion
    layerInfo.version = "1";

    auto info = dir.info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    layerInfo.info = nlohmann::json(*info);
    auto data = QByteArray::fromStdString(nlohmann::json(layerInfo).dump());

    QByteArray dataSizeBytes;

    QDataStream dataSizeStream(&dataSizeBytes, QIODevice::WriteOnly);
    dataSizeStream.setVersion(QDataStream::Qt_5_10);
    dataSizeStream.setByteOrder(QDataStream::LittleEndian);
    dataSizeStream << quint32(data.size());

    Q_ASSERT(dataSizeStream.status() == QDataStream::Status::Ok);

    if (layer.write(dataSizeBytes) < 0) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(data) < 0) {
        return LINGLONG_ERR(layer);
    }

    layer.close();

    // compress data with erofs
    const auto &compressedFilePath = this->workDir / "tmp.erofs";
    const auto &ignoreRegex = QString{ "--exclude-regex=minified*" };
    // 使用-b统一指定block size为4096(2^12), 避免不同系统的兼容问题
    // loongarch64默认使用(16384)2^14, 在x86和arm64不受支持, 会导致无法推包
    auto ret = utils::command::Cmd("mkfs.erofs")
                 .exec({ "-z" + compressor,
                         "-b4096",
                         compressedFilePath.string().c_str(),
                         ignoreRegex,
                         dir.absolutePath() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    ret = utils::command::Cmd("sh").exec(
      { "-c", QString("cat %1 >> %2").arg(compressedFilePath.string().c_str(), layerFilePath) });
    if (!ret) {
        LINGLONG_ERR(ret);
    }

    auto result = LayerFile::New(layerFilePath);
    Q_ASSERT(result.has_value());

    return result;
}

// 判断fd是否可在其他进程读取
bool LayerPackager::isFileReadable(const std::string &path) const
{
    LINGLONG_TRACE("check file permission");
    std::ifstream f(path);
    return f.good();
}

// 手动将fd保存为文件，可以避免文件无权限的问题
utils::error::Result<void> LayerPackager::copyFile(LayerFile &file,
                                                   const std::string &toPath,
                                                   const int64_t offset) const
{
    LINGLONG_TRACE("save file");
    file.seek(offset);
    std::ofstream ofs(toPath);
    char buff[4096];
    while (true) {
        auto n = file.read(buff, 4096);
        if (n < 0) {
            return LINGLONG_ERR("Failed to read from layer file: " + file.errorString());
        }
        if (n == 0) {
            break;
        }
        ofs.write(buff, n);
        if (ofs.fail()) {
            return LINGLONG_ERR("Failed to write to temporary file");
        }
    }
    ofs.close();
    if (ofs.fail()) {
        return LINGLONG_ERR("Failed to close temporary file");
    }
    return LINGLONG_OK;
}

utils::error::Result<LayerDir> LayerPackager::unpack(LayerFile &file)
{
    LINGLONG_TRACE("unpack layer file");

    auto unpackDir = QDir((this->workDir / "unpack").string().c_str());
    unpackDir.mkpath(".");

    auto offset = file.binaryDataOffset();
    if (!offset) {
        return LINGLONG_ERR(offset);
    }
    auto fdPath = QString{ "/proc/%1/fd/%2" }.arg(::getpid()).arg(file.handle());
    auto isReadable = this->isFileReadable(fdPath.toStdString());
    // 判断erofsfuse命令是否存在
    auto erofsFuseExistsRet = this->checkErofsFuseExists();
    if (!erofsFuseExistsRet.has_value()) {
        return LINGLONG_ERR(erofsFuseExistsRet);
    }
    if (*erofsFuseExistsRet) {
        // 如果fd可读，则直接使用erofsfuse命令+offset参数挂载
        auto fuseOffset = QString::number(*offset);
        // 如果fd不可读，则将fd保存为文件，再使用erofsfuse命令挂载
        if (!isReadable) {
            fdPath = (this->workDir / "layer.erofs").string().c_str();
            auto ret = this->copyFile(file, fdPath.toStdString(), *offset);
            if (!ret) {
                return LINGLONG_ERR(ret);
            }
            fuseOffset = "0";
        }
        auto ret = utils::command::Cmd("erofsfuse")
                     .exec({ "--offset=" + fuseOffset, fdPath, unpackDir.absolutePath() });
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        this->isMounted = true;
        return unpackDir.absolutePath();
    }
    // 判断fsck.erofs命令是否存在，fsck.erofs是erofs-utils的命令，可用于解压erofs文件
    // 在旧版本中fsck.erofs不支持offset参数，所以需要提前将erofs文件复制到临时目录
    auto erofsFscExistsRet = utils::command::Cmd("fsck.erofs").exists();
    if (!erofsFscExistsRet.has_value()) {
        return LINGLONG_ERR(erofsFscExistsRet);
    }
    if (*erofsFscExistsRet) {
        fdPath = (this->workDir / "layer.erofs").string().c_str();
        auto ret = this->copyFile(file, fdPath.toStdString(), *offset);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        auto cmdRet = utils::command::Cmd("fsck.erofs")
                        .exec({ "--extract=" + unpackDir.absolutePath(), fdPath });
        if (!cmdRet) {
            return LINGLONG_ERR(cmdRet);
        }
        return unpackDir.absolutePath();
    }
    return LINGLONG_ERR(
      "erofsfuse or fsck.erofs not found, please install erofs-utils or erofsfuse",
      utils::error::ErrorCode::AppInstallErofsNotFound);
}

utils::error::Result<void> LayerPackager::setCompressor(const QString &compressor) noexcept
{
    this->compressor = compressor;
    return LINGLONG_OK;
}

utils::error::Result<bool> LayerPackager::checkErofsFuseExists() const
{
    return utils::command::Cmd("erofsfuse").exists();
}

} // namespace linglong::package
