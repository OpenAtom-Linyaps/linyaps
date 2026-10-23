// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../mocks/uab_file_mock.h"
#include "common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/uab_signature.h"
#include "linglong/package/elf_handler.h"
#include "linglong/package/uab_file.h"
#include "linglong/package/uab_packager.h"
#include "linglong/utils/cmd.h"

#include <gelf.h>
#include <libelf.h>

#include <QCryptographicHash>
#include <QFile>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

__attribute__((used, section(".note.uab.sig"), aligned(4))) const auto linglongUabSignature =
  linglong::common::uab::signatureNote;

using namespace linglong;

namespace linglong::package {

class UabFileTest : public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        testDir = std::make_unique<TempDir>("linglong-uab-file-test-");
        uabFile = (testDir->path() / "test.uab").string();
        ASSERT_EQ(linglongUabSignature.digest[0], '!');
        std::filesystem::copy_file("/proc/self/exe", uabFile);
        auto uab = ElfHandler::create(uabFile);
        ASSERT_TRUE(uab.has_value()) << "Failed to create uab file";
        // 添加bundle section
        std::string bundleFile = (testDir->path() / "bundle.erofs").string();
        {
            std::error_code ec;
            std::filesystem::create_directories(testDir->path() / "bundle/layers/test/binary", ec);
            ASSERT_FALSE(ec) << "Failed to create test directory";
            std::string helloFilePath =
              (testDir->path() / "bundle" / "layers" / "test" / "binary" / "info.json").string();
            std::ofstream tmpFile(helloFilePath);
            tmpFile << "Hello, World!";
            tmpFile.close();
            const auto bundleDir = testDir->path() / "bundle";
            auto ret = utils::Cmd("mkfs.erofs").exec({ bundleFile.c_str(), bundleDir.c_str() });
            ASSERT_TRUE(ret.has_value()) << "Failed to create erofs file" << ret.error().message();
            auto ret2 = (*uab)->addSection("linglong.bundle", bundleFile);
            ASSERT_TRUE(ret2.has_value()) << ret2.error().message();
        }
        // 新添加 meta section
        {
            api::types::v1::PackageInfoV2 packageInfo;
            packageInfo.name = "hello";
            packageInfo.version = "1";
            packageInfo.description = "hello world";
            packageInfo.id = "hello";
            packageInfo.version = "1";
            api::types::v1::UabMetaInfo meta;
            meta.version = api::types::v1::Version::The1;
            meta.uuid = "b2f33c7b-615c-4d7d-9181-e1a22010a749";
            meta.onlyApp = true;
            meta.sections.bundle = "linglong.bundle";
            meta.layers.push_back(api::types::v1::UabLayer{ packageInfo, false });
            // 计算bundle哈希值
            QFile bundle{ bundleFile.c_str() };
            if (!bundle.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
                ASSERT_TRUE(false) << "Failed to open bundle file";
            }
            QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
            if (!cryptor.addData(&bundle)) {
                ASSERT_TRUE(false) << "Failed to add data to cryptor";
            }
            bundle.close();
            const auto digest = cryptor.result().toHex().toStdString();

            meta.digest = digest;

            std::ofstream jsonFile(testDir->path() / "info.json");
            jsonFile << nlohmann::json(meta).dump();
            jsonFile.close();
            auto ret = (*uab)->addSection("linglong.meta", testDir->path() / "info.json");
            ASSERT_TRUE(ret.has_value()) << "Failed to add meta section";

            QFile metaFile{ (testDir->path() / "info.json").c_str() };
            ASSERT_TRUE(metaFile.open(QIODevice::ReadOnly | QIODevice::ExistingOnly));
            QCryptographicHash metaCryptor{ QCryptographicHash::Sha256 };
            ASSERT_TRUE(metaCryptor.addData(&metaFile));
            const auto metaDigest = metaCryptor.result().toHex().toStdString();
            auto writeMetaDigest =
              (*uab)->writeSectionData(std::string{ common::uab::signatureSection },
                                       common::uab::digestOffset,
                                       metaDigest.data(),
                                       metaDigest.size());
            ASSERT_TRUE(writeMetaDigest.has_value()) << writeMetaDigest.error().message();
        }
        // 再添加sign section
        {
            auto signDir = testDir->path() / "sign";
            std::error_code ec;
            std::filesystem::create_directories(signDir, ec);
            ASSERT_FALSE(ec) << "Failed to create sign directory";
            std::string helloFilePath = signDir / "hello";
            std::ofstream tmpFile(helloFilePath);
            tmpFile << "Hello, World!";
            tmpFile.close();
            auto ret = utils::Cmd("tar").exec(
              { "-cvf", (testDir->path() / "sign.tar").c_str(), "-C", signDir.c_str(), "." });
            ASSERT_TRUE(ret.has_value()) << "Failed to create tar file";
            auto ret2 = (*uab)->addSection("linglong.bundle.sign", testDir->path() / "sign.tar");
            ASSERT_TRUE(ret2.has_value()) << "Failed to add sign section" << ret2.error().message();
        }
    }

    static void TearDownTestCase() { testDir.reset(); }

    static std::string uabFile;
    static std::unique_ptr<TempDir> testDir;
};

std::string UabFileTest::uabFile;
std::unique_ptr<TempDir> UabFileTest::testDir;

std::string calculateDigest(std::string_view data)
{
    QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
    cryptor.addData(data.data(), static_cast<int>(data.size()));
    return cryptor.result().toHex().toStdString();
}

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream{ path };
    return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
}

TEST_F(UabFileTest, UnpackFuseOffset)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }

    // 初始化UABFile对象
    auto uab = linglong::package::UABFile::loadFromFile(uabFile);
    ASSERT_TRUE(uab.has_value()) << "Failed to load uab file";
    const auto unpackPath = testDir->path() / "unpack-offset";
    auto unpackRet = (*uab)->unpack(unpackPath);
    ASSERT_TRUE(unpackRet.has_value())
      << "Failed to unpack uab file" << unpackRet.error().message();

    ASSERT_TRUE(std::filesystem::exists(unpackPath / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << unpackPath / "info.json";
}

TEST_F(UabFileTest, UnpackFuse)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }
    {
        auto ret = utils::Cmd("erofsfuse").exists();
        if (!ret) {
#ifdef GTEST_SKIP
            GTEST_SKIP() << "Skipping this test.";
#else
            return;
#endif
        }
    }
    auto uab = MockUabFile(uabFile);
    uab.wrapIsFileReadableFunc = []([[maybe_unused]] const std::string &path) {
        return false;
    };
    uab.wrapMkdirDirFunc = [](const std::string &path) -> utils::error::Result<void> {
        std::filesystem::create_directories(path);
        return LINGLONG_OK;
    };
    const auto unpackPath = testDir->path() / "unpack-fuse";
    auto unpackRet = uab.unpack(unpackPath);
    ASSERT_TRUE(unpackRet.has_value()) << "Failed to unpack uab file";

    ASSERT_TRUE(std::filesystem::exists(unpackPath / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << unpackPath / "info.json";
}

TEST_F(UabFileTest, UnpackFsck)
{
    auto uab = MockUabFile(uabFile);
    uab.wrapCheckCommandExistsFunc = [](const std::string &command) {
        if (command == "erofsfuse") {
            return false;
        }
        return true;
    };
    const auto unpackPath = testDir->path() / "unpack-fsck";
    auto unpackRet = uab.unpack(unpackPath);
    ASSERT_TRUE(unpackRet.has_value()) << "Failed to unpack uab file";

    ASSERT_TRUE(std::filesystem::exists(unpackPath / "layers/test/binary/info.json"))
      << "'info.json' not found in unpack dir" << unpackPath / "info.json";
}

TEST_F(UabFileTest, Verify)
{
    auto uab = MockUabFile(uabFile);
    auto verifyRet = uab.verify();
    ASSERT_TRUE(verifyRet.has_value()) << "Failed to verify uab file";
    ASSERT_TRUE(*verifyRet) << "Verify failed";
}

TEST_F(UabFileTest, VerifyRejectsMismatchedMetaSignature)
{
    TempDir modifiedDir{ "linglong-uab-modified-" };
    const auto modifiedUab = modifiedDir.path() / "mismatched-meta.uab";
    std::filesystem::copy_file(uabFile,
                               modifiedUab,
                               std::filesystem::copy_options::overwrite_existing);
    {
        auto elf = ElfHandler::create(modifiedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        const std::string mismatchedDigest(common::uab::digestSize, '0');
        auto writeRet = (*elf)->writeSectionData(std::string{ common::uab::signatureSection },
                                                 common::uab::digestOffset,
                                                 mismatchedDigest.data(),
                                                 mismatchedDigest.size());
        ASSERT_TRUE(writeRet.has_value()) << writeRet.error().message();
    }

    auto uab = UABFile::loadFromFile(modifiedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    ASSERT_TRUE(verifyRet.has_value()) << verifyRet.error().message();
    EXPECT_FALSE(*verifyRet);
}

TEST_F(UabFileTest, VerifyRejectsTamperedMetaSection)
{
    TempDir modifiedDir{ "linglong-uab-modified-" };
    const auto modifiedUab = modifiedDir.path() / "tampered-meta.uab";
    std::filesystem::copy_file(uabFile,
                               modifiedUab,
                               std::filesystem::copy_options::overwrite_existing);
    {
        auto elf = ElfHandler::create(modifiedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        constexpr char tamperedByte{ '!' };
        auto writeRet =
          (*elf)->writeSectionData("linglong.meta", 0, &tamperedByte, sizeof(tamperedByte));
        ASSERT_TRUE(writeRet.has_value()) << writeRet.error().message();
    }

    auto uab = UABFile::loadFromFile(modifiedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    ASSERT_TRUE(verifyRet.has_value()) << verifyRet.error().message();
    EXPECT_FALSE(*verifyRet);
}

TEST_F(UabFileTest, VerifyRejectsAuthenticatedInvalidBundleDigest)
{
    TempDir modifiedDir{ "linglong-uab-modified-" };
    const auto modifiedUab = modifiedDir.path() / "invalid-bundle-digest.uab";
    std::filesystem::copy_file(uabFile,
                               modifiedUab,
                               std::filesystem::copy_options::overwrite_existing);

    auto metaData = readFile(testDir->path() / "info.json");
    auto meta = nlohmann::json::parse(metaData).get<api::types::v1::UabMetaInfo>();
    const auto digestOffset = metaData.find(meta.digest);
    ASSERT_NE(digestOffset, std::string::npos);
    const std::string invalidDigest(common::uab::digestSize, 'g');
    metaData.replace(digestOffset, meta.digest.size(), invalidDigest);
    const auto metaDigest = calculateDigest(metaData);

    {
        auto elf = ElfHandler::create(modifiedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        auto writeMetaRet =
          (*elf)->writeSectionData("linglong.meta", 0, metaData.data(), metaData.size());
        ASSERT_TRUE(writeMetaRet.has_value()) << writeMetaRet.error().message();
        auto writeSignatureRet =
          (*elf)->writeSectionData(std::string{ common::uab::signatureSection },
                                   common::uab::digestOffset,
                                   metaDigest.data(),
                                   metaDigest.size());
        ASSERT_TRUE(writeSignatureRet.has_value()) << writeSignatureRet.error().message();
    }

    auto uab = UABFile::loadFromFile(modifiedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    EXPECT_FALSE(verifyRet.has_value());
}

TEST_F(UabFileTest, VerifyRejectsAuthenticatedMissingBundleSection)
{
    TempDir modifiedDir{ "linglong-uab-modified-" };
    const auto modifiedUab = modifiedDir.path() / "missing-bundle-section.uab";
    std::filesystem::copy_file(uabFile,
                               modifiedUab,
                               std::filesystem::copy_options::overwrite_existing);

    auto metaData = readFile(testDir->path() / "info.json");
    constexpr std::string_view bundleSection{ "linglong.bundle" };
    constexpr std::string_view missingSection{ "missing.section" };
    static_assert(bundleSection.size() == missingSection.size());
    const auto sectionOffset = metaData.find(bundleSection);
    ASSERT_NE(sectionOffset, std::string::npos);
    metaData.replace(sectionOffset, bundleSection.size(), missingSection);
    const auto metaDigest = calculateDigest(metaData);

    {
        auto elf = ElfHandler::create(modifiedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        auto writeMetaRet =
          (*elf)->writeSectionData("linglong.meta", 0, metaData.data(), metaData.size());
        ASSERT_TRUE(writeMetaRet.has_value()) << writeMetaRet.error().message();
        auto writeSignatureRet =
          (*elf)->writeSectionData(std::string{ common::uab::signatureSection },
                                   common::uab::digestOffset,
                                   metaDigest.data(),
                                   metaDigest.size());
        ASSERT_TRUE(writeSignatureRet.has_value()) << writeSignatureRet.error().message();
    }

    auto uab = UABFile::loadFromFile(modifiedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    EXPECT_FALSE(verifyRet.has_value());
}

TEST_F(UabFileTest, VerifyRejectsMismatchedBundleDigest)
{
    TempDir modifiedDir{ "linglong-uab-modified-" };
    const auto modifiedUab = modifiedDir.path() / "mismatched-bundle.uab";
    std::filesystem::copy_file(uabFile,
                               modifiedUab,
                               std::filesystem::copy_options::overwrite_existing);
    {
        auto elf = ElfHandler::create(modifiedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        constexpr char tamperedByte{ '!' };
        auto writeRet =
          (*elf)->writeSectionData("linglong.bundle", 0, &tamperedByte, sizeof(tamperedByte));
        ASSERT_TRUE(writeRet.has_value()) << writeRet.error().message();
    }

    auto uab = UABFile::loadFromFile(modifiedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    ASSERT_TRUE(verifyRet.has_value()) << verifyRet.error().message();
    EXPECT_FALSE(*verifyRet);
}

TEST_F(UabFileTest, VerifyWithoutBundleSignSection)
{
    TempDir unsignedDir{ "linglong-uab-unsigned-" };
    const auto unsignedUab = unsignedDir.path() / "unsigned.uab";
    std::filesystem::copy_file("/proc/self/exe",
                               unsignedUab,
                               std::filesystem::copy_options::overwrite_existing);

    auto calculateDigest = [](const std::filesystem::path &path) {
        QFile file{ path.c_str() };
        EXPECT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly));
        QCryptographicHash cryptor{ QCryptographicHash::Sha256 };
        EXPECT_TRUE(cryptor.addData(&file));
        return cryptor.result().toHex().toStdString();
    };

    {
        auto elf = ElfHandler::create(unsignedUab);
        ASSERT_TRUE(elf.has_value()) << elf.error().message();
        const auto bundleFile = testDir->path() / "bundle.erofs";
        const auto metaFile = testDir->path() / "info.json";
        ASSERT_TRUE((*elf)->addSection("linglong.bundle", bundleFile));
        ASSERT_TRUE((*elf)->addSection(std::string{ common::uab::metaSection }, metaFile));

        const auto digest = calculateDigest(metaFile);
        auto writeRet = (*elf)->writeSectionData(std::string{ common::uab::signatureSection },
                                                 common::uab::digestOffset,
                                                 digest.data(),
                                                 digest.size());
        ASSERT_TRUE(writeRet.has_value()) << writeRet.error().message();
    }

    auto uab = UABFile::loadFromFile(unsignedUab);
    ASSERT_TRUE(uab.has_value()) << uab.error().message();
    auto verifyRet = (*uab)->verify();
    ASSERT_TRUE(verifyRet.has_value()) << verifyRet.error().message();
    EXPECT_TRUE(*verifyRet);
}

TEST(UabSignatureTest, ParseMetaSignatureNote)
{
    const auto bytes = std::string_view{
        reinterpret_cast<const char *>(&common::uab::signatureNote),
        sizeof(common::uab::signatureNote),
    };
    auto digest = common::uab::parseSignatureNote(bytes);
    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(digest->size(), common::uab::digestSize);
}

TEST(UabSignatureTest, ParseRejectsExtraNote)
{
    std::string bytes{ reinterpret_cast<const char *>(&common::uab::signatureNote),
                       sizeof(common::uab::signatureNote) };
    bytes.append(reinterpret_cast<const char *>(&common::uab::signatureNote),
                 sizeof(common::uab::signatureNote));
    EXPECT_FALSE(common::uab::parseSignatureNote(bytes).has_value());
}

TEST(UabSignatureTest, ParseRejectsWrongName)
{
    auto note = common::uab::signatureNote;
    note.name[0] = 'x';
    const auto bytes = std::string_view{ reinterpret_cast<const char *>(&note), sizeof(note) };
    EXPECT_FALSE(common::uab::parseSignatureNote(bytes).has_value());
}

TEST_F(UabFileTest, ExtractSignData)
{
    auto uab = MockUabFile(uabFile);
    auto ret = uab.unpack(testDir->path() / "unpack-sign");
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack uab file " << ret.error().message();
    auto extractSignDataRet = uab.extractSignData(testDir->path() / "sign-data");
    ASSERT_TRUE(extractSignDataRet.has_value())
      << "Failed to extract sign data " << extractSignDataRet.error().message();
    auto signDataDir = *extractSignDataRet / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    ASSERT_TRUE(std::filesystem::exists(signDataDir / "hello"))
      << "Failed to extract sign data " << signDataDir / "hello";
    std::ifstream helloFile(signDataDir / "hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
    std::error_code ec;
    std::filesystem::remove_all(*extractSignDataRet, ec);
    ASSERT_FALSE(ec) << "Failed to remove extractSignDataRet" << ec.message();
}

TEST_F(UabFileTest, ExtractSignDataRetriesShortWrites)
{
    auto uab = MockUabFile(uabFile);
    uab.wrapWriteDataFunc = [](int fd, const void *data, std::size_t size) {
        constexpr std::size_t MaxWriteSize = 7;
        return ::write(fd, data, std::min(size, MaxWriteSize));
    };

    auto extractSignDataRet = uab.extractSignData(testDir->path() / "sign-data-short-write");
    ASSERT_TRUE(extractSignDataRet.has_value())
      << "Failed to extract sign data " << extractSignDataRet.error().message();

    auto signDataDir = *extractSignDataRet / "entries" / "share" / "deepin-elf-verify" / ".elfsign";
    std::ifstream helloFile(signDataDir / "hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    EXPECT_EQ(buffer.str(), "Hello, World!");
}

TEST_F(UabFileTest, GetSectionHeaderRejectsZeroShnum)
{
    TempDir dir{ "linglong-uab-zero-shnum-" };
    auto corruptedUab = dir.path() / "corrupted-zero-shnum.uab";
    std::error_code ec;
    std::filesystem::copy_file(uabFile, corruptedUab, ec);
    ASSERT_FALSE(ec);

    {
        std::fstream fs(corruptedUab, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shnumOffset = (ident[EI_CLASS] == ELFCLASS64) ? 60 : 48;
        uint16_t zeroShnum = 0;
        fs.seekp(shnumOffset);
        fs.write(reinterpret_cast<const char *>(&zeroShnum), sizeof(zeroShnum));
        ASSERT_TRUE(fs.good());
    }

    MockUabFile uab(corruptedUab);
    auto secRet = uab.testGetSectionHeader("linglong.meta");
    ASSERT_FALSE(secRet.has_value());
    EXPECT_THAT(secRet.error().message(),
                ::testing::HasSubstr("invalid section header count or string table index"));
}

TEST_F(UabFileTest, GetSectionHeaderRejectsOutOfBoundsShstrndx)
{
    TempDir dir{ "linglong-uab-oob-shstrndx-" };
    auto corruptedUab = dir.path() / "corrupted-oob-shstrndx.uab";
    std::error_code ec;
    std::filesystem::copy_file(uabFile, corruptedUab, ec);
    ASSERT_FALSE(ec);

    {
        std::fstream fs(corruptedUab, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shstrndxOffset = (ident[EI_CLASS] == ELFCLASS64) ? 62 : 50;
        uint16_t largeShstrndx = 0xFFFE;
        fs.seekp(shstrndxOffset);
        fs.write(reinterpret_cast<const char *>(&largeShstrndx), sizeof(largeShstrndx));
        ASSERT_TRUE(fs.good());
    }

    MockUabFile uab(corruptedUab);
    auto secRet = uab.testGetSectionHeader("linglong.meta");
    ASSERT_FALSE(secRet.has_value());
    EXPECT_THAT(secRet.error().message(),
                ::testing::HasSubstr("invalid section header count or string table index"));
}

TEST_F(UabFileTest, GetSectionHeaderSkipsMalformedSectionNamesSafely)
{
    TempDir dir{ "linglong-uab-corrupt-sname-" };
    auto corruptedUab = dir.path() / "corrupted-sname.uab";
    std::error_code ec;
    std::filesystem::copy_file(uabFile, corruptedUab, ec);
    ASSERT_FALSE(ec);

    {
        std::fstream fs(corruptedUab, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);
        std::streamoff e_shoff_offset = is64 ? 40 : 32;
        std::streamoff e_shentsize_offset = is64 ? 58 : 46;

        fs.seekg(e_shoff_offset);
        uint64_t shoff = 0;
        if (is64) {
            fs.read(reinterpret_cast<char *>(&shoff), sizeof(uint64_t));
        } else {
            uint32_t shoff32 = 0;
            fs.read(reinterpret_cast<char *>(&shoff32), sizeof(uint32_t));
            shoff = shoff32;
        }

        fs.seekg(e_shentsize_offset);
        uint16_t shentsize = 0;
        fs.read(reinterpret_cast<char *>(&shentsize), sizeof(uint16_t));

        ASSERT_GT(shoff, 0);
        ASSERT_GT(shentsize, 0);

        // Section 1's sh_name is at (shoff + shentsize).
        std::streamoff sec1ShNameOffset = shoff + shentsize;
        uint32_t invalidShName = 0x7FFFFFFF;
        fs.seekp(sec1ShNameOffset);
        fs.write(reinterpret_cast<const char *>(&invalidShName), sizeof(invalidShName));
        ASSERT_TRUE(fs.good());
    }

    MockUabFile uab(corruptedUab);
    auto secRet = uab.testGetSectionHeader("linglong.meta");
    ASSERT_TRUE(secRet.has_value()) << secRet.error().message();

    auto nonexistentRet = uab.testGetSectionHeader("nonexistent.section.test");
    ASSERT_FALSE(nonexistentRet.has_value());
    EXPECT_THAT(nonexistentRet.error().message(), ::testing::HasSubstr("couldn't found section"));
}

TEST_F(UabFileTest, ReadSectionDataRejectsExceedingFileSize)
{
    TempDir dir{ "linglong-uab-file-size-" };
    auto corruptedUab = dir.path() / "corrupted-file-size.uab";
    std::error_code ec;
    std::filesystem::copy_file(uabFile, corruptedUab, ec);
    ASSERT_FALSE(ec);

    const auto fileSize = std::filesystem::file_size(corruptedUab);
    ASSERT_GT(fileSize, 0);

    std::streamoff shSizeFileOffset = -1;
    bool is64 = false;
    {
        int fd = ::open(corruptedUab.c_str(), O_RDONLY);
        ASSERT_GE(fd, 0);
        elf_version(EV_CURRENT);
        Elf *elf = elf_begin(fd, ELF_C_READ, nullptr);
        ASSERT_NE(elf, nullptr);

        GElf_Ehdr ehdr;
        ASSERT_NE(gelf_getehdr(elf, &ehdr), nullptr);

        is64 = (gelf_getclass(elf) == ELFCLASS64);

        size_t shstrndx = 0;
        ASSERT_EQ(elf_getshdrstrndx(elf, &shstrndx), 0);

        Elf_Scn *scn = nullptr;
        while ((scn = elf_nextscn(elf, scn)) != nullptr) {
            GElf_Shdr shdr;
            ASSERT_NE(gelf_getshdr(scn, &shdr), nullptr);
            const char *name = elf_strptr(elf, shstrndx, shdr.sh_name);
            if (name != nullptr && std::string_view(name) == "linglong.meta") {
                const auto scnIdx = elf_ndxscn(scn);
                const std::streamoff shdrOffset = ehdr.e_shoff + scnIdx * ehdr.e_shentsize;
                const std::streamoff sizeFieldOffset =
                  is64 ? offsetof(Elf64_Shdr, sh_size) : offsetof(Elf32_Shdr, sh_size);
                shSizeFileOffset = shdrOffset + sizeFieldOffset;
                break;
            }
        }
        elf_end(elf);
        ::close(fd);
    }
    ASSERT_GT(shSizeFileOffset, 0);

    {
        std::fstream fs(corruptedUab, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        fs.seekp(shSizeFileOffset);
        if (is64) {
            const uint64_t forgedSize = fileSize + 4096;
            fs.write(reinterpret_cast<const char *>(&forgedSize), sizeof(forgedSize));
        } else {
            ASSERT_LE(fileSize + 4096, std::numeric_limits<uint32_t>::max());
            const uint32_t forgedSize = static_cast<uint32_t>(fileSize + 4096);
            fs.write(reinterpret_cast<const char *>(&forgedSize), sizeof(forgedSize));
        }
        ASSERT_TRUE(fs.good());
    }

    MockUabFile uab(corruptedUab);
    auto ret = uab.testReadSectionData("linglong.meta", 0, fileSize + 1024);
    ASSERT_FALSE(ret.has_value());
    EXPECT_THAT(ret.error().message(),
                ::testing::HasSubstr("section data exceeds physical file size"));
}

TEST_F(UabFileTest, ExtractSignDataUsesCloexecAndCleansUp)
{
    auto uab = MockUabFile(uabFile);
    int capturedFlags = -1;
    uab.wrapWriteDataFunc =
      [&capturedFlags](int fd, const void *data, std::size_t size) -> ssize_t {
        capturedFlags = ::fcntl(fd, F_GETFD);
        return ::write(fd, data, size);
    };

    auto extractRet = uab.extractSignData(testDir->path() / "sign-data-cloexec");
    ASSERT_TRUE(extractRet.has_value()) << extractRet.error().message();

    EXPECT_NE(capturedFlags, -1);
    EXPECT_NE(capturedFlags & FD_CLOEXEC, 0);

    auto tarPath =
      *extractRet / "entries" / "share" / "deepin-elf-verify" / ".elfsign" / "sign.tar";
    EXPECT_FALSE(std::filesystem::exists(tarPath));
}

} // namespace linglong::package
