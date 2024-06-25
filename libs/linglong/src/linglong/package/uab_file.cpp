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

namespace linglong::package {

utils::error::Result<std::unique_ptr<UABFile>> UABFile::loadFromFile(const QString &input)
{
    struct EnableMaker : public UABFile
    {
        using UABFile::UABFile;
    };

    LINGLONG_TRACE("load uab file")
    auto file = std::make_unique<EnableMaker>();

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
    if (!mountPoint.isEmpty()) {
        auto ret = utils::command::Exec("umount", { mountPoint });
        if (!ret) {
            qCritical() << "failed to umount " << mountPoint << ", please umount it manually";
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

utils::error::Result<QDir> UABFile::mountUab() noexcept
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
    QDir destination{ QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) };
    QDir uabDir = destination.absoluteFilePath("linglong" % QDir::separator() % "UAB"
                                               % QDir::separator() % QString::fromStdString(uuid));

    if (!uabDir.mkpath(".")) {
        return LINGLONG_ERR(QString{ "failed mkpath %1" }.arg(uabDir.absolutePath()));
    }

    auto ret = utils::command::Exec(
      "erofsfuse",
      { QString{ "--offset=%1" }.arg(bundleOffset), fileName(), uabDir.absolutePath() });
    if (!ret) {
        return LINGLONG_ERR(ret.error());
    }

    this->mountPoint = uabDir.absolutePath();
    qDebug() << "erofsfuse output:" << *ret;

    return mountPoint;
}

} // namespace linglong::package
