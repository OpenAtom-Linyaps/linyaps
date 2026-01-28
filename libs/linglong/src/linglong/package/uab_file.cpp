// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_file.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/error.h"
#include "linglong/common/formatter.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::package {

utils::error::Result<std::unique_ptr<UABFile>> UABFile::loadFromFile(int fd) noexcept
{
    LINGLONG_TRACE("load uab file from fd")

    struct EnableMaker : public UABFile
    {
        using UABFile::UABFile;
    };

    auto file = std::make_unique<EnableMaker>();

    if (!file->open(fd, QIODevice::ReadOnly, FileHandleFlag::AutoCloseHandle)) {
        return LINGLONG_ERR(fmt::format("open uab failed: {}", file->errorString()));
    }

    elf_version(EV_CURRENT);
    auto *elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (elf == nullptr) {
        return LINGLONG_ERR(fmt::format("libelf err: {}", elf_errmsg(errno)));
    }

    file->e = elf;
    return file;
}

UABFile::~UABFile()
{
    if (!m_mountPoint.empty()) {
        auto ret = utils::Cmd("fusermount").exec({ "-z", "-u", m_mountPoint.c_str() });
        if (!ret) {
            LogE("failed to umount {}, please umount it manually", m_mountPoint);
        }
    }
    if (!m_unpackPath.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(m_unpackPath).parent_path(), ec);
        if (ec) {
            LogE("failed to remove {}, please remove it manually", m_unpackPath);
        }
    }
    if (this->e) {
        elf_end(this->e);
        this->e = nullptr;
    }
}

utils::error::Result<GElf_Shdr> UABFile::getSectionHeader(const QString &section) const noexcept
{
    LINGLONG_TRACE("get uab section header")

    size_t shdrstrndx{ 0 };
    if (elf_getshdrstrndx(this->e, &shdrstrndx) == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to get section header index of bundle: {}", elf_errmsg(errno)));
    }

    Elf_Scn *scn{ nullptr };
    while ((scn = elf_nextscn(this->e, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            return LINGLONG_ERR(
              fmt::format("failed to get section header of bundle: {}", elf_errmsg(errno)));
        }

        auto *sname = elf_strptr(this->e, shdrstrndx, shdr.sh_name);
        if (section.compare(sname) == 0) {
            return shdr;
        }
    }

    return LINGLONG_ERR(fmt::format("couldn't found section {}", section));
}

utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
UABFile::getMetaInfo() noexcept
{
    LINGLONG_TRACE("get metaInfo from uab")

    if (this->metaInfo) {
        return { *(this->metaInfo) };
    }

    auto metaSh = getSectionHeader("linglong.meta");
    if (!metaSh) {
        return LINGLONG_ERR(metaSh.error());
    }

    seek(metaSh->sh_offset);
    auto backToHead = utils::finally::finally([this] {
        seek(0);
    });

    auto metaInfo = read(metaSh->sh_size).toStdString();
    if (metaInfo.empty()) {
        return LINGLONG_ERR(fmt::format("couldn't read metaInfo from uab: {}", errorString()));
    }

    nlohmann::json content;
    try {
        content = nlohmann::json::parse(metaInfo);
    } catch (nlohmann::json::parse_error &e) {
        return LINGLONG_ERR("parsing metaInfo error", e);
    } catch (...) {
        return LINGLONG_ERR("unknown exception has been catch");
    }

    this->metaInfo =
      std::make_unique<api::types::v1::UabMetaInfo>(content.get<api::types::v1::UabMetaInfo>());
    return *(this->metaInfo);
}

utils::error::Result<bool> UABFile::verify() noexcept
{
    LINGLONG_TRACE("verify uab")

    auto metaInfoRet = getMetaInfo();
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet.error());
    }

    auto metaInfo = metaInfoRet->get();
    auto expectedDigest = metaInfo.digest;
    auto bundleSection = QString::fromStdString(metaInfo.sections.bundle);
    auto bundleSh = getSectionHeader(bundleSection);
    if (!bundleSh) {
        return LINGLONG_ERR(
          fmt::format("couldn't find bundle section which named {}", bundleSection));
    }

    std::array<char, 4096> buf{};
    std::string digest;
    QCryptographicHash cryptor{ QCryptographicHash::Sha256 };

    seek(bundleSh->sh_offset);
    auto backToHead = utils::finally::finally([this] {
        seek(0);
    });

    auto bundleLength = bundleSh->sh_size;
    auto readBytes = buf.size();
    int bytesRead{ 0 };
    while ((bytesRead = read(buf.data(), readBytes)) != 0) {
        if (bytesRead == -1) {
            return LINGLONG_ERR(fmt::format("read error: {}", errorString()));
        }
        cryptor.addData(
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
          QByteArrayView{ buf.data(), bytesRead }
#else
          buf.data(),
          bytesRead
#endif
        );
        bundleLength -= bytesRead;
        if (bundleLength <= 0) {
            digest = cryptor.result().toHex().toStdString();
            break;
        }
    }

    return (expectedDigest == digest);
}

utils::error::Result<std::filesystem::path> UABFile::unpack() noexcept
{
    LINGLONG_TRACE("unpack uab bundle")

    auto metaInfoRet = getMetaInfo();
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet.error());
    }

    auto bundleSh = getSectionHeader(QString::fromStdString(metaInfo->sections.bundle));
    if (!bundleSh) {
        return LINGLONG_ERR(bundleSh.error());
    }

    auto bundleOffset = bundleSh->sh_offset;
    auto metaInfo = metaInfoRet->get();
    auto uuid = metaInfo.uuid;

    auto offset = bundleOffset;
    auto uabFile = QString{ "/proc/%1/fd/%2" }.arg(::getpid()).arg(handle());

    auto dirName = "linglong-uab-" + QUuid::createUuid().toString(QUuid::Id128).toStdString();
    // 优先使用/var/tmp目录，避免tmpfs内存不足
    auto *tmpDir = std::getenv("LINGLONG_TMPDIR");
    auto unpackPath = std::filesystem::path(tmpDir ? tmpDir : "/var/tmp") / dirName / "unpack";
    auto ret = this->mkdirDir(unpackPath);
    if (!ret) {
        // 如果/var/tmp目录无权限创建，则使用临时目录
        unpackPath = std::filesystem::temp_directory_path() / dirName / "unpack";
        ret = this->mkdirDir(unpackPath);
        if (!ret) {
            return LINGLONG_ERR("failed to create directory " + unpackPath.string(), ret);
        }
    }

    // 如果erofsfuse存在，则使用erofsfuse挂载
    if (this->checkCommandExists("erofsfuse")) {
        auto isFileReadable = this->isFileReadable(uabFile.toStdString());
        if (!isFileReadable) {
            offset = 0;
            uabFile = (unpackPath.parent_path() / "bundle.erofs").c_str();
            auto ret = this->saveErofsToFile(unpackPath.parent_path() / "bundle.erofs");
            if (!ret) {
                return LINGLONG_ERR(ret.error());
            }
        }
        auto ret = utils::Cmd("erofsfuse")
                     .exec(std::vector<std::string>{ fmt::format("--offset={}", offset),
                                                     uabFile.toStdString(),
                                                     unpackPath.string() });
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
        this->m_mountPoint = unpackPath;
        this->m_unpackPath = unpackPath;
        return unpackPath;
    }
    // 如果erofsfuse不存在，则使用fsck.erofs解压erofs文件
    if (this->checkCommandExists("fsck.erofs")) {
        uabFile = (unpackPath.parent_path() / "bundle.erofs").c_str();
        auto ret = this->saveErofsToFile(unpackPath.parent_path() / "bundle.erofs");
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
        auto cmdRet = utils::Cmd("fsck.erofs")
                        .exec(std::vector<std::string>{ "--extract=" + unpackPath.string(),
                                                        uabFile.toStdString() });
        if (!cmdRet) {
            return LINGLONG_ERR(cmdRet);
        }
        this->m_unpackPath = unpackPath;
        return unpackPath;
    }
    return LINGLONG_ERR(
      "erofsfuse or fsck.erofs not found, please install erofs-utils or erofsfuse",
      utils::error::ErrorCode::AppInstallErofsNotFound);
}

utils::error::Result<std::filesystem::path> UABFile::extractSignData() noexcept
{
    LINGLONG_TRACE("extract sign data from uab")
    if (m_unpackPath.empty()) {
        return LINGLONG_ERR("uab is not mounted");
    }

    auto signSection = getSectionHeader("linglong.bundle.sign");
    if (!signSection) {
        LogI("couldn't get sign data: {} skip", signSection.error());
        return {};
    }

    std::error_code ec;
    auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return LINGLONG_ERR(ec.message().c_str());
    }

    constexpr std::string_view charSet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<> dist(0, charSet.size() - 1);

    std::string tmpName;
    for (std::size_t i = 0; i < 8; ++i) {
        tmpName += charSet[dist(rng)];
    }

    auto root = tempDir / ("uab-temp-layer-" + tmpName);

    auto destination = root / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    if (!std::filesystem::create_directories(destination, ec) && ec) {
        return LINGLONG_ERR(ec.message().c_str());
    }

    auto tarFile = destination / "sign.tar";
    auto tarFd = ::open(tarFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tarFd == -1) {
        return LINGLONG_ERR(
          fmt::format("open {} failed: {}", tarFile, common::error::errorString(errno)));
    }

    auto removeTar = utils::finally::finally([&tarFd, &tarFile] {
        // return in while loop, tar file isn't closed
        if (tarFd != -1) {
            ::close(tarFd);
        }

        std::error_code ec;
        if (!std::filesystem::remove(tarFile, ec) && ec) {
            LogW("failed to remove {}: {}", tarFile.string(), ec.message());
        }
    });

    seek(signSection->sh_offset);
    auto backToHead = utils::finally::finally([this] {
        seek(0);
    });

    auto selfFd = handle();
    auto totalBytes = signSection->sh_size;
    std::array<unsigned char, 4096> buf{};
    while (totalBytes > 0) {
        auto bytesRead = totalBytes > buf.size() ? buf.size() : totalBytes;
        auto readBytes = ::read(selfFd, buf.data(), bytesRead);
        if (readBytes == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            return LINGLONG_ERR(
              fmt::format("read from sign section error: {}", common::error::errorString(errno)));
        }

        while (true) {
            auto writeBytes = ::write(tarFd, buf.data(), readBytes);
            if (writeBytes == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                }
                return LINGLONG_ERR(
                  fmt::format("write to sign.tar error: {}", common::error::errorString(errno)));
            }

            if (writeBytes != readBytes) {
                return LINGLONG_ERR("write to sign.tar failed: byte mismatch");
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
        tarFd = -1; // no need to try twice
        return LINGLONG_ERR(
          fmt::format("failed to close tar: {}", common::error::errorString(errno)));
    }
    tarFd = -1;

    auto ret = utils::Cmd("tar").exec({ "-xf", tarFile.string(), "-C", destination.string() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return root;
}

bool UABFile::isFileReadable(const std::string &path) const
{
    std::ifstream f(path);
    return f.good();
}

utils::error::Result<void> UABFile::saveErofsToFile(const std::string &path)
{
    LINGLONG_TRACE("save erofs file");

    auto metaInfoRet = getMetaInfo();
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet.error());
    }

    auto bundleSh = getSectionHeader(QString::fromStdString(metaInfo->sections.bundle));
    if (!bundleSh) {
        return LINGLONG_ERR(bundleSh.error());
    }
    seek(bundleSh->sh_offset);
    auto backToHead = utils::finally::finally([this] {
        seek(0);
    });
    auto bundleLength = bundleSh->sh_size;
    // 流式保存bundleSection到path
    std::ofstream ofs(path, std::ios::binary);
    std::array<char, 4096> buf{};
    while (bundleLength > 0) {
        auto readBytes = bundleLength > buf.size() ? buf.size() : bundleLength;
        auto bytesRead = ::read(handle(), buf.data(), readBytes);
        if (bytesRead == -1) {
            return LINGLONG_ERR(
              fmt::format("read from bundle section error: {}", common::error::errorString(errno)));
        }
        ofs.write(buf.data(), bytesRead);
        if (ofs.fail()) {
            return LINGLONG_ERR(fmt::format("write {} failed", path));
        }
        bundleLength -= bytesRead;
    }
    ofs.close();
    if (ofs.fail()) {
        return LINGLONG_ERR(fmt::format("close {} failed", path));
    }
    return LINGLONG_OK;
}

utils::error::Result<void> UABFile::mkdirDir(const std::string &path) noexcept
{
    LINGLONG_TRACE("mkdir dir" + path);
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create directory" + path, ec);
    }
    return LINGLONG_OK;
}

bool UABFile::checkCommandExists(const std::string &command) const
{
    return utils::Cmd(command).exists();
}

} // namespace linglong::package
