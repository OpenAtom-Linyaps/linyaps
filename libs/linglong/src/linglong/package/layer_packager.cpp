/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/utils/cmd.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"

#include <QDataStream>
#include <QSaveFile>
#include <QSysInfo>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace linglong::package {

LayerPackager::LayerPackager()
{
    // maybe refactor on later
    auto ret = this->initWorkDir();
    if (!ret) {
        LogE("init work dir failed");
    }
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
    auto uuid = QUuid::createUuid().toString(QUuid::Id128);
    auto dirName = "linglong-layer-workdir-" + uuid.toStdString();
    auto *tmpDir = std::getenv("LINGLONG_TMPDIR");
    auto dirPath = std::filesystem::path(tmpDir ? tmpDir : "/var/tmp") / dirName;
    auto ret = this->mkdirDir(dirPath);
    if (!ret.has_value()) {
        // 如果/var/tmp目录无权限创建，则使用临时目录
        dirPath = std::filesystem::temp_directory_path() / dirName;
        ret = this->mkdirDir(dirPath);
        if (!ret) {
            LogE("failed to set work dir: {}", ret.error());
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
        auto ret = utils::Cmd("fusermount")
                     .exec({ "-z", "-u", (this->workDir / "unpack").string().c_str() });
        if (!ret) {
            LogW("failed to umount {}, please umount it manually",
                 (this->workDir / "unpack").string());
        }
    }
    if (!std::filesystem::remove_all(this->workDir)) {
        LogE("failed to remove {}", this->workDir);
    }
}

utils::error::Result<QSharedPointer<LayerFile>>
LayerPackager::pack(const LayerDir &dir, const QString &layerFilePath) const
{
    LINGLONG_TRACE("pack layer");

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

    // compress data with erofs
    const auto &compressedFilePath = this->workDir / "tmp.erofs";
    // 使用-b统一指定block size为4096(2^12), 避免不同系统的兼容问题
    // loongarch64默认使用(16384)2^14, 在x86和arm64不受支持, 会导致无法推包
    auto ret = utils::Cmd("mkfs.erofs")
                 .exec(std::vector<std::string>{ "-z" + compressor.toStdString(),
                                                 "-b4096",
                                                 compressedFilePath.string(),
                                                 "--exclude-regex=minified*",
                                                 dir.path() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    QFile compressedFile(QString::fromStdString(compressedFilePath.string()));
    if (!compressedFile.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(compressedFile.errorString().toStdString());
    }

    QSaveFile layer(layerFilePath);
    layer.setDirectWriteFallback(false);
    if (!layer.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(layer.errorString().toStdString());
    }

    auto writeBytes = [&layer](const char *bytes, qint64 size) -> utils::error::Result<void> {
        qint64 writtenTotal = 0;
        while (writtenTotal < size) {
            const auto written = layer.write(bytes + writtenTotal, size - writtenTotal);
            if (written <= 0) {
                return LINGLONG_ERR(layer.errorString().toStdString());
            }
            writtenTotal += written;
        }
        return LINGLONG_OK;
    };

    const auto &number = magicNumber();
    if (auto writeResult = writeBytes(number.constData(), number.size()); !writeResult) {
        return LINGLONG_ERR(writeResult);
    }
    if (auto writeResult = writeBytes(dataSizeBytes.constData(), dataSizeBytes.size());
        !writeResult) {
        return LINGLONG_ERR(writeResult);
    }
    if (auto writeResult = writeBytes(data.constData(), data.size()); !writeResult) {
        return LINGLONG_ERR(writeResult);
    }

    std::array<char, 64 * 1024> buffer{};
    while (true) {
        const auto bytesRead = compressedFile.read(buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return LINGLONG_ERR(compressedFile.errorString().toStdString());
        }
        if (bytesRead == 0) {
            break;
        }
        if (auto writeResult = writeBytes(buffer.data(), bytesRead); !writeResult) {
            return LINGLONG_ERR(writeResult);
        }
    }

    if (!layer.commit()) {
        return LINGLONG_ERR(layer.errorString().toStdString());
    }

    auto result = LayerFile::New(layerFilePath);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return result;
}

// 判断fd是否可在其他进程读取
bool LayerPackager::isFileReadable(const std::string &path) const
{
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
            return LINGLONG_ERR("Failed to read from layer file: "
                                + file.errorString().toStdString());
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

    auto unpackDir = this->workDir / "unpack";
    auto res = utils::ensureDirectory(unpackDir);
    if (!res) {
        return LINGLONG_ERR(res);
    }

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
        auto ret =
          utils::Cmd("erofsfuse")
            .exec({ "--offset=" + fuseOffset.toStdString(), fdPath.toStdString(), unpackDir });
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        this->isMounted = true;
        return unpackDir;
    }
    // 判断fsck.erofs命令是否存在，fsck.erofs是erofs-utils的命令，可用于解压erofs文件
    // 在旧版本中fsck.erofs不支持offset参数，所以需要提前将erofs文件复制到临时目录
    auto erofsFscExistsRet = utils::Cmd("fsck.erofs").exists();
    if (erofsFscExistsRet) {
        fdPath = (this->workDir / "layer.erofs").string().c_str();
        auto ret = this->copyFile(file, fdPath.toStdString(), *offset);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        auto cmdRet = utils::Cmd("fsck.erofs")
                        .exec({ "--extract=" + unpackDir.string(), fdPath.toStdString() });
        if (!cmdRet) {
            return LINGLONG_ERR(cmdRet);
        }
        return unpackDir;
    }
    return LINGLONG_ERR(
      "erofsfuse or fsck.erofs not found, please install erofs-utils or erofsfuse",
      utils::error::ErrorCode::AppInstallErofsNotFound);
}

void LayerPackager::setCompressor(const QString &compressor) noexcept
{
    this->compressor = compressor;
}

utils::error::Result<bool> LayerPackager::checkErofsFuseExists() const
{
    return utils::Cmd("erofsfuse").exists();
}

} // namespace linglong::package
