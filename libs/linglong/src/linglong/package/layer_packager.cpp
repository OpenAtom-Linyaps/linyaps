/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/common/error.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <QDataStream>
#include <QSysInfo>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <fcntl.h>
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

    QFile layer(layerFilePath);
    if (layer.exists()) {
        layer.remove();
    }

    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR(layer.errorString().toStdString());
    }

    const auto &number = magicNumber();
    if (layer.write(number) < 0) {
        return LINGLONG_ERR(layer.errorString().toStdString());
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
        return LINGLONG_ERR(layer.errorString().toStdString());
    }

    if (layer.write(data) < 0) {
        return LINGLONG_ERR(layer.errorString().toStdString());
    }

    layer.close();

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

    auto res = utils::concatFile(compressedFilePath, layerFilePath.toStdString());
    if (!res) {
        return LINGLONG_ERR(res);
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

utils::error::Result<void> LayerPackager::extractSignData(LayerFile &file,
                                                          const std::filesystem::path &signDir)
{
    LINGLONG_TRACE("extract sign data from layer");

    const auto &number = magicNumber();
    file.seek(number.size());

    QDataStream metaLenStream(&file);
    metaLenStream.setByteOrder(QDataStream::LittleEndian);
    quint32 metaLen = 0;
    metaLenStream >> metaLen;

    auto metaRawData = file.read(metaLen);
    auto metaJson = nlohmann::json::parse(metaRawData.toStdString());
    auto erofsSize = metaJson.value("erofs_size", 0ULL);
    auto signSize = metaJson.value("sign_size", 0ULL);

    if (signSize == 0) {
        return LINGLONG_OK;
    }

    auto signOffset = static_cast<int64_t>(number.size() + sizeof(quint32) + metaLen + erofsSize);

    std::error_code ec;
    auto destination = signDir / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    if (!std::filesystem::create_directories(destination, ec) && ec) {
        return LINGLONG_ERR(ec.message().c_str());
    }

    auto tarFile = destination / "sign.tar";
    auto tarFd = ::open(tarFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tarFd == -1) {
        return LINGLONG_ERR(
          fmt::format("open {} failed: {}", tarFile.string(), common::error::errorString(errno)));
    }

    auto removeTar = utils::finally::finally([&tarFd, &tarFile] {
        if (tarFd != -1) {
            ::close(tarFd);
        }

        std::error_code ec;
        if (!std::filesystem::remove(tarFile, ec) && ec) {
            LogW("failed to remove {}: {}", tarFile.string(), ec.message());
        }
    });

    file.seek(signOffset);
    auto selfFd = file.handle();

    auto totalBytes = signSize;
    std::array<unsigned char, 4096> buf{};
    while (totalBytes > 0) {
        auto bytesToRead = totalBytes > buf.size() ? buf.size() : totalBytes;
        auto readBytes = ::read(selfFd, buf.data(), bytesToRead);
        if (readBytes == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            return LINGLONG_ERR(
              fmt::format("read sign data error: {}", common::error::errorString(errno)));
        }

        while (true) {
            auto writeBytes = ::write(tarFd, buf.data(), readBytes);
            if (writeBytes == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                }
                return LINGLONG_ERR(
                  fmt::format("write sign.tar error: {}", common::error::errorString(errno)));
            }

            if (writeBytes != readBytes) {
                return LINGLONG_ERR("write sign.tar failed: byte mismatch");
            }

            totalBytes -= writeBytes;
            break;
        }
    }

    if (::fsync(tarFd) == -1) {
        return LINGLONG_ERR(
          fmt::format("fsync sign.tar error: {}", common::error::errorString(errno)));
    }

    if (::close(tarFd) == -1) {
        tarFd = -1;
        return LINGLONG_ERR(
          fmt::format("failed to close tar: {}", common::error::errorString(errno)));
    }
    tarFd = -1;

    auto ret = utils::Cmd("tar").exec({ "-xf", tarFile.string(), "-C", destination.string() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
