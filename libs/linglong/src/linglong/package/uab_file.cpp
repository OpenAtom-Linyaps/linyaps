// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_file.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/error.h"
#include "linglong/common/formatter.h"
#include "linglong/common/uab_signature.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <QCryptographicHash>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong::package {

utils::error::Result<std::unique_ptr<UABFile>>
UABFile::loadFromFile(const std::filesystem::path &path) noexcept
{
    LINGLONG_TRACE("load uab file from path")

    struct helper : public UABFile
    {
    };

    auto file = std::make_unique<helper>();
    file->fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (file->fd == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to open uab file {}: {}", path, common::error::errorString(errno)));
    }

    elf_version(EV_CURRENT);
    auto *elf = elf_begin(file->fd, ELF_C_READ, nullptr);
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
    if (this->e) {
        elf_end(this->e);
        this->e = nullptr;
    }
    if (this->fd != -1) {
        if (::close(this->fd) == -1) {
            LogW("failed to close fd {}: {}", fd, common::error::errorString(errno));
        }
        this->fd = -1;
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

utils::error::Result<std::string> UABFile::readSectionData(const QString &section,
                                                           std::size_t offset,
                                                           std::size_t size) noexcept
{
    LINGLONG_TRACE("read uab section data")

    auto sectionHeader = getSectionHeader(section);
    if (!sectionHeader) {
        return LINGLONG_ERR(sectionHeader.error());
    }
    if (sectionHeader->sh_type == SHT_NOBITS || offset > sectionHeader->sh_size
        || size > sectionHeader->sh_size - offset) {
        return LINGLONG_ERR(fmt::format("data exceeds section {}", section));
    }

    // sh_size comes from untrusted file content. A corrupted or malicious
    // section header can claim far more data than the bundle holds, so the
    // section content is required to lie within the file before allocating.
    struct stat fileStat{};
    if (::fstat(this->fd, &fileStat) == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to stat uab file: {}", common::error::errorString(errno)));
    }
    if (fileStat.st_size < 0
        || static_cast<std::uintmax_t>(sectionHeader->sh_offset)
          > static_cast<std::uintmax_t>(fileStat.st_size)
        || sectionHeader->sh_size
          > static_cast<std::uintmax_t>(fileStat.st_size) - sectionHeader->sh_offset) {
        return LINGLONG_ERR(fmt::format("section {} exceeds the uab file size", section));
    }

    std::string data(size, '\0');
    std::size_t totalBytes{ 0 };
    while (totalBytes < size) {
        const auto bytesRead = ::pread(fd,
                                       data.data() + totalBytes,
                                       size - totalBytes,
                                       sectionHeader->sh_offset + offset + totalBytes);
        if (bytesRead == -1 && errno == EINTR) {
            continue;
        }
        if (bytesRead == -1) {
            return LINGLONG_ERR(fmt::format("failed to read section {}: {}",
                                            section,
                                            common::error::errorString(errno)));
        }
        if (bytesRead == 0) {
            return LINGLONG_ERR(fmt::format("unexpected end of section {}", section));
        }
        totalBytes += static_cast<std::size_t>(bytesRead);
    }
    return data;
}

utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
UABFile::parseMetaInfo(std::string_view metaData) noexcept
{
    LINGLONG_TRACE("parse metaInfo")

    try {
        auto content = nlohmann::json::parse(metaData);
        this->metaInfo =
          std::make_unique<api::types::v1::UabMetaInfo>(content.get<api::types::v1::UabMetaInfo>());
    } catch (nlohmann::json::exception &e) {
        return LINGLONG_ERR("parsing metaInfo error", e);
    } catch (...) {
        return LINGLONG_ERR("unknown exception has been catch");
    }

    return *(this->metaInfo);
}

utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
UABFile::getMetaInfo() noexcept
{
    LINGLONG_TRACE("get metaInfo from uab")

    if (this->metaInfo) {
        return { *(this->metaInfo) };
    }

    const auto metaSection = QString::fromStdString(std::string{ common::uab::metaSection });
    auto metaSh = getSectionHeader(metaSection);
    if (!metaSh) {
        return LINGLONG_ERR(metaSh.error());
    }
    auto metaData = readSectionData(metaSection, 0, metaSh->sh_size);
    if (!metaData) {
        return LINGLONG_ERR(metaData.error());
    }
    if (metaData->empty()) {
        return LINGLONG_ERR("metaInfo is empty");
    }
    return parseMetaInfo(*metaData);
}

utils::error::Result<bool> UABFile::verify() noexcept
{
    LINGLONG_TRACE("verify uab")

    const auto signatureSection =
      QString::fromStdString(std::string{ common::uab::signatureSection });
    auto signatureSh = getSectionHeader(signatureSection);
    if (!signatureSh) {
        return LINGLONG_ERR(signatureSh.error());
    }
    if (signatureSh->sh_size != sizeof(common::uab::MetaSignatureNote)) {
        return LINGLONG_ERR(fmt::format("section {} is invalid", signatureSection));
    }

    auto noteDataRet = readSectionData(signatureSection, 0, signatureSh->sh_size);
    if (!noteDataRet) {
        return LINGLONG_ERR(noteDataRet.error());
    }
    const auto expectedMetaDigest = common::uab::parseSignatureNote(*noteDataRet);
    if (!expectedMetaDigest) {
        return LINGLONG_ERR(fmt::format("section {} has an invalid note layout", signatureSection));
    }
    if (!common::uab::isDigest(*expectedMetaDigest)) {
        return LINGLONG_ERR(fmt::format("section {} has an invalid digest", signatureSection));
    }

    const auto calculateSectionDigest =
      [&](const GElf_Shdr &sectionHeader) -> utils::error::Result<std::string> {
        std::array<char, 4096> buffer{};
        QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
        std::size_t offset{ 0 };
        std::size_t remaining = sectionHeader.sh_size;
        while (remaining > 0) {
            const auto requested = std::min(remaining, buffer.size());
            const auto bytesRead =
              ::pread(fd, buffer.data(), requested, sectionHeader.sh_offset + offset);
            if (bytesRead == -1 && errno == EINTR) {
                continue;
            }
            if (bytesRead == -1) {
                return LINGLONG_ERR(
                  fmt::format("read error: {}", common::error::errorString(errno)));
            }
            if (bytesRead == 0) {
                return LINGLONG_ERR("unexpected end of section");
            }
            cryptor.addData(
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
              QByteArrayView{ buffer.data(), bytesRead }
#else
              buffer.data(),
              bytesRead
#endif
            );
            remaining -= static_cast<std::size_t>(bytesRead);
            offset += static_cast<std::size_t>(bytesRead);
        }
        return cryptor.result().toHex().toStdString();
    };

    const auto metaSection = QString::fromStdString(std::string{ common::uab::metaSection });
    auto metaSh = getSectionHeader(metaSection);
    if (!metaSh) {
        return LINGLONG_ERR(metaSh.error());
    }
    auto metaData = readSectionData(metaSection, 0, metaSh->sh_size);
    if (!metaData) {
        return LINGLONG_ERR(metaData.error());
    }
    if (metaData->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return LINGLONG_ERR("linglong.meta is too large");
    }

    QCryptographicHash metaCryptor{ QCryptographicHash::Sha256 };
    metaCryptor.addData(metaData->data(), static_cast<int>(metaData->size()));
    if (metaCryptor.result().toHex().toStdString() != *expectedMetaDigest) {
        return false;
    }

    auto metaInfoRet = parseMetaInfo(*metaData);
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet.error());
    }
    const auto &metaInfo = metaInfoRet->get();
    if (!common::uab::isDigest(metaInfo.digest)) {
        return LINGLONG_ERR("linglong.meta contains an invalid bundle digest");
    }

    auto bundleSh = getSectionHeader(QString::fromStdString(metaInfo.sections.bundle));
    if (!bundleSh) {
        return LINGLONG_ERR(bundleSh.error());
    }
    if (bundleSh->sh_type == SHT_NOBITS) {
        return LINGLONG_ERR("bundle section has no file data");
    }
    auto actualBundleDigest = calculateSectionDigest(*bundleSh);
    if (!actualBundleDigest) {
        return LINGLONG_ERR(actualBundleDigest.error());
    }

    return *actualBundleDigest == metaInfo.digest;
}

utils::error::Result<void> UABFile::unpack(const std::filesystem::path &destination) noexcept
{
    LINGLONG_TRACE("unpack uab bundle")

    auto metaInfoRet = getMetaInfo();
    if (!metaInfoRet) {
        return LINGLONG_ERR(metaInfoRet.error());
    }

    const auto &metaInfo = metaInfoRet->get();
    auto bundleSh = getSectionHeader(QString::fromStdString(metaInfo.sections.bundle));
    if (!bundleSh) {
        return LINGLONG_ERR(bundleSh.error());
    }

    auto bundleOffset = bundleSh->sh_offset;
    auto offset = bundleOffset;
    auto uabFile = std::filesystem::path{
        fmt::format("/proc/{}/fd/{}", static_cast<long long>(::getpid()), fd)
    };

    auto ret = this->mkdirDir(destination);
    if (!ret) {
        return LINGLONG_ERR("failed to create directory " + destination.string(), ret);
    }

    // 如果erofsfuse存在，则使用erofsfuse挂载
    if (this->checkCommandExists("erofsfuse")) {
        auto isFileReadable = this->isFileReadable(uabFile.string());
        if (!isFileReadable) {
            offset = 0;
            uabFile = destination.parent_path() / "bundle.erofs";
            auto ret = this->saveErofsToFile(uabFile);
            if (!ret) {
                return LINGLONG_ERR(ret.error());
            }
        }
        auto ret = utils::Cmd("erofsfuse")
                     .exec(std::vector<std::string>{ fmt::format("--offset={}", offset),
                                                     uabFile.string(),
                                                     destination.string() });
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
        this->m_mountPoint = destination;
        return LINGLONG_OK;
    }
    // 如果erofsfuse不存在，则使用fsck.erofs解压erofs文件
    if (this->checkCommandExists("fsck.erofs")) {
        uabFile = destination.parent_path() / "bundle.erofs";
        auto ret = this->saveErofsToFile(uabFile);
        if (!ret) {
            return LINGLONG_ERR(ret.error());
        }
        auto cmdRet = utils::Cmd("fsck.erofs")
                        .exec(std::vector<std::string>{ "--extract=" + destination.string(),
                                                        uabFile.string() });
        if (!cmdRet) {
            return LINGLONG_ERR(cmdRet);
        }
        return LINGLONG_OK;
    }
    return LINGLONG_ERR(
      "erofsfuse or fsck.erofs not found, please install erofs-utils or erofsfuse",
      utils::error::ErrorCode::AppInstallErofsNotFound);
}

utils::error::Result<std::filesystem::path>
UABFile::extractSignData(const std::filesystem::path &destination) noexcept
{
    LINGLONG_TRACE("extract sign data from uab")
    auto signSection = getSectionHeader("linglong.bundle.sign");
    if (!signSection) {
        LogI("couldn't get sign data: {} skip", signSection.error());
        return {};
    }

    auto signDir = destination / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    std::error_code ec;
    if (!std::filesystem::create_directories(signDir, ec) && ec) {
        return LINGLONG_ERR(ec.message().c_str());
    }

    auto tarFile = signDir / "sign.tar";
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

    auto totalBytes = signSection->sh_size;
    std::size_t offset{ 0 };
    std::array<unsigned char, 4096> buf{};
    while (totalBytes > 0) {
        auto bytesRead = totalBytes > buf.size() ? buf.size() : totalBytes;
        auto readBytes = ::pread(fd, buf.data(), bytesRead, signSection->sh_offset + offset);
        if (readBytes == -1) {
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
            return LINGLONG_ERR(
              fmt::format("read from sign section error: {}", common::error::errorString(errno)));
        }
        if (readBytes == 0) {
            return LINGLONG_ERR("unexpected end of sign section");
        }

        std::size_t writtenBytes{ 0 };
        while (writtenBytes < static_cast<std::size_t>(readBytes)) {
            auto writeBytes = this->writeData(tarFd,
                                              buf.data() + writtenBytes,
                                              static_cast<std::size_t>(readBytes) - writtenBytes);
            if (writeBytes == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                }
                return LINGLONG_ERR(
                  fmt::format("write to sign.tar error: {}", common::error::errorString(errno)));
            }

            if (writeBytes == 0) {
                return LINGLONG_ERR("write to sign.tar failed: zero bytes written");
            }

            writtenBytes += static_cast<std::size_t>(writeBytes);
        }

        totalBytes -= static_cast<std::size_t>(readBytes);
        offset += static_cast<std::size_t>(readBytes);
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

    auto ret = utils::Cmd("tar").exec({ "-xf", tarFile.string(), "-C", signDir.string() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return destination;
}

bool UABFile::isFileReadable(const std::string &path) const
{
    std::ifstream f(path);
    return f.good();
}

ssize_t UABFile::writeData(int fd, const void *data, std::size_t size) const noexcept
{
    return ::write(fd, data, size);
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
    auto bundleLength = bundleSh->sh_size;
    std::size_t offset{ 0 };
    // 流式保存bundleSection到path
    std::ofstream ofs(path, std::ios::binary);
    std::array<char, 4096> buf{};
    while (bundleLength > 0) {
        auto readBytes = bundleLength > buf.size() ? buf.size() : bundleLength;
        auto bytesRead = ::pread(fd, buf.data(), readBytes, bundleSh->sh_offset + offset);
        if (bytesRead == -1 && errno == EINTR) {
            continue;
        }
        if (bytesRead == -1) {
            return LINGLONG_ERR(
              fmt::format("read from bundle section error: {}", common::error::errorString(errno)));
        }
        if (bytesRead == 0) {
            return LINGLONG_ERR("unexpected end of bundle section");
        }
        ofs.write(buf.data(), bytesRead);
        if (ofs.fail()) {
            return LINGLONG_ERR(fmt::format("write {} failed", path));
        }
        bundleLength -= bytesRead;
        offset += static_cast<std::size_t>(bytesRead);
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
