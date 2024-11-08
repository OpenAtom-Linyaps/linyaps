// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_file.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/command/env.h"
#include "linglong/utils/finally/finally.h"

#include <nlohmann/json.hpp>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <random>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::package {

/**
 * In the package_manager.cpp file, the method installFromUAB attempts to move a lambda to
 * QCoreApplication::instance() via QMetaObject::invokeMethod, which has a object of type
 * std::unique_ptr<uabFile> that created from this method. However, when compiling this project with
 * Qt 5.11, the type of lambda (rvalue reference) is lost during parameter passing to Qt's internal
 * method, so the compiler tries to call the copy constructor of this lambda. std::unique_ptr has a
 * deleted copy constructor, so it compiles failed. The temporary resolution is that returning a
 * std::shared_pointer<UABFile>, if linglong depends on a minimal version of Qt above 5.11, change
 * the type of returned value to std::unique_ptr<UABFile>.
 **/
utils::error::Result<std::shared_ptr<UABFile>> UABFile::loadFromFile(const QString &input)
{
    struct EnableMaker : public UABFile
    {
        using UABFile::UABFile;
    };

    LINGLONG_TRACE("load uab file")
    auto file = std::make_shared<EnableMaker>();

    file->setFileName(input);
    if (!file->open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        return LINGLONG_ERR(QString{ "open uab failed: %1" }.arg(file->errorString()));
    }

    auto fd = file->handle();

    elf_version(EV_CURRENT);
    auto *elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (elf == nullptr) {
        return LINGLONG_ERR(QString{ "libelf err:" }.arg(elf_errmsg(errno)));
    }

    file->e = elf;

    return file;
}

UABFile::~UABFile()
{
    if (!mountPoint.empty()) {
        auto ret = utils::command::Exec("umount", { mountPoint.c_str() });
        if (!ret) {
            qCritical() << "failed to umount " << mountPoint.c_str()
                        << ", please umount it manually";
        }
    }

    elf_end(this->e);
}

utils::error::Result<GElf_Shdr> UABFile::getSectionHeader(const QString &section) const noexcept
{
    LINGLONG_TRACE("get uab section header")

    size_t shdrstrndx{ 0 };
    if (elf_getshdrstrndx(this->e, &shdrstrndx) == -1) {
        return LINGLONG_ERR(
          QString{ "failed to get section header index of bundle: %1" }.arg(elf_errmsg(errno)));
    }

    Elf_Scn *scn{ nullptr };
    while ((scn = elf_nextscn(this->e, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) == nullptr) {
            return LINGLONG_ERR(
              QString{ "failed to get section header of bundle: %1" }.arg(elf_errmsg(errno)));
        }

        auto *sname = elf_strptr(this->e, shdrstrndx, shdr.sh_name);
        if (section.compare(sname) == 0) {
            return shdr;
        }
    }

    return LINGLONG_ERR(QString{ "couldn't found section %1" }.arg(section));
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
        return LINGLONG_ERR(QString{ "couldn't read metaInfo from uab: %1" }.arg(errorString()));
    }

    nlohmann::json content;
    try {
        content = nlohmann::json::parse(metaInfo);
    } catch (nlohmann::json::parse_error &e) {
        return LINGLONG_ERR(QString{ "parsing metaInfo error: %1" }.arg(e.what()));
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
          QString{ "couldn't find bundle section which named %1" }.arg(bundleSection));
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
            return LINGLONG_ERR(QString{ "read error: %1" }.arg(errorString()));
        }

        cryptor.addData(buf.data(), bytesRead);
        bundleLength -= bytesRead;
        if (bundleLength <= 0) {
            digest = cryptor.result().toHex().toStdString();
            break;
        }
    }

    return (expectedDigest == digest);
}

utils::error::Result<std::filesystem::path> UABFile::mountUab() noexcept
{
    LINGLONG_TRACE("mount uab bundle")

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
    auto destination = std::filesystem::path{
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation).toStdString()
    };
    auto uabDir = destination / "linglong" / "UAB" / uuid;

    std::error_code ec;
    if (!std::filesystem::create_directories(uabDir, ec) && ec) {
        return LINGLONG_ERR(QString{ "failed create directory" } % uabDir.c_str() % ":"
                            % ec.message().c_str());
    }

    auto ret = utils::command::Exec(
      "erofsfuse",
      QStringList{ QString{ "--offset=%1" }.arg(bundleOffset), fileName(), uabDir.c_str() });
    if (!ret) {
        return LINGLONG_ERR(ret.error());
    }

    this->mountPoint = uabDir;
    qDebug() << "erofsfuse output:" << *ret;

    return mountPoint;
}

utils::error::Result<std::filesystem::path> UABFile::extractSignData() noexcept
{
    LINGLONG_TRACE("extract sign data from uab")
    if (mountPoint.empty()) {
        return LINGLONG_ERR("uab is not mounted");
    }

    auto signSection = getSectionHeader("linglong.bundle.sign");
    if (!signSection) {
        qInfo() << "couldn't get sign data:" << signSection.error().message() << "skip";
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

    auto destination = root / "entries" / "share" / "deepin-elf-verify" / "linglong" / ".elfsign";
    if (!std::filesystem::create_directories(destination, ec) && ec) {
        return LINGLONG_ERR(ec.message().c_str());
    }

    auto tarFile = destination / "sign.tar";
    auto tarFd = ::open(tarFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (tarFd == -1) {
        return LINGLONG_ERR(QString{ "open" } % tarFile.c_str() % " failed:" % strerror(errno));
    }

    auto removeTar = utils::finally::finally([&tarFd, &tarFile] {
        // return in while loop, tar file isn't closed
        if (tarFd != -1) {
            ::close(tarFd);
        }

        std::error_code ec;
        if (!std::filesystem::remove(tarFile, ec) && ec) {
            qWarning() << "remove" << tarFile.c_str() << "failed:" << ec.message().c_str();
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
            return LINGLONG_ERR(QString{ "read from sign section error:" } % ::strerror(errno));
        }

        while (true) {
            auto writeBytes = ::write(tarFd, buf.data(), readBytes);
            if (writeBytes == -1) {
                if (errno == EINTR) {
                    errno = 0;
                    continue;
                }
                return LINGLONG_ERR(QString{ "write to sign.tar error:" } % ::strerror(errno));
            }

            if (writeBytes != readBytes) {
                return LINGLONG_ERR("write to sign.tar failed: byte mismatch");
            }

            totalBytes -= writeBytes;
            break;
        }
    }

    if (::fsync(tarFd) == -1) {
        return LINGLONG_ERR(QString{ "fsync sign.tar error: " } % ::strerror(errno));
    }

    if (::close(tarFd) == -1) {
        tarFd = -1; // no need to try twice
        return LINGLONG_ERR(QString{ "failed to close tar: " } % ::strerror(errno));
    }
    tarFd = -1;

    auto ret =
      utils::command::Exec("tar", QStringList{ "-xf", tarFile.c_str(), "-C", destination.c_str() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return root;
}

} // namespace linglong::package
