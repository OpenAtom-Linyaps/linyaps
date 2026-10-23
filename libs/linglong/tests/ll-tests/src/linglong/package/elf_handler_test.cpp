/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/elf_handler.h"

#include <common/tempdir.h>
#include <gelf.h>
#include <libelf.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

using namespace linglong::package;

namespace {

std::vector<char> readBinaryFile(const std::filesystem::path &filePath)
{
    std::ifstream fs(filePath, std::ios::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("failed to open binary file for reading: " + filePath.string());
    }
    return std::vector<char>((std::istreambuf_iterator<char>(fs)),
                             std::istreambuf_iterator<char>());
}

bool hasOpenFileDescriptorForPath(const std::filesystem::path &targetPath)
{
    std::error_code ec;
    auto canonicalTarget = std::filesystem::weakly_canonical(targetPath, ec);
    if (ec) {
        canonicalTarget = targetPath;
    }

    auto fdDir = std::filesystem::path("/proc/self/fd");
    if (!std::filesystem::exists(fdDir, ec)) {
        throw std::runtime_error("/proc/self/fd is not accessible");
    }

    bool hasEntries = false;
    for (const auto &entry : std::filesystem::directory_iterator(fdDir, ec)) {
        if (ec) {
            throw std::runtime_error("failed to iterate /proc/self/fd: " + ec.message());
        }
        hasEntries = true;
        std::error_code readEc;
        auto linkTarget = std::filesystem::read_symlink(entry.path(), readEc);
        if (!readEc && linkTarget == canonicalTarget) {
            return true;
        }
    }
    if (!hasEntries) {
        throw std::runtime_error("no entries found when iterating /proc/self/fd");
    }
    return false;
}

bool hasMemoryMappingForPath(const std::filesystem::path &targetPath)
{
    std::error_code ec;
    auto canonicalTarget = std::filesystem::weakly_canonical(targetPath, ec);
    if (ec) {
        canonicalTarget = targetPath;
    }

    auto mapsFile = std::filesystem::path("/proc/self/maps");
    if (!std::filesystem::exists(mapsFile, ec)) {
        throw std::runtime_error("/proc/self/maps is not accessible");
    }

    std::ifstream maps(mapsFile);
    if (!maps.is_open()) {
        throw std::runtime_error("failed to open /proc/self/maps");
    }

    std::string line;
    const auto targetStr = canonicalTarget.string();
    while (std::getline(maps, line)) {
        if (line.find(targetStr) != std::string::npos) {
            return true;
        }
    }
    return false;
}

TEST(ElfHandlerTest, CreateSucceedsOnAnyFile)
{
    TempDir dir;
    auto file = dir.path() / "any";
    std::ofstream(file) << "hello";
    EXPECT_TRUE(ElfHandler::create(file).has_value());
}

TEST(ElfHandlerTest, AddSectionNameTooLongFails)
{
    TempDir dir;
    auto elf = dir.path() / "copy";
    // copy the current executable as a valid ELF
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());
    const std::string longName(33, 'a');
    auto result = (*handler)->addSection(longName, "data", 4);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("section name too long"));
}

TEST(ElfHandlerTest, AddSectionNonElfFileFails)
{
    TempDir dir;
    auto elf = dir.path() / "not-elf";
    std::ofstream(elf) << "this is definitely not an elf file";

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());
    auto result = (*handler)->addSection(".ll-test", "data", 4);
    ASSERT_FALSE(result.has_value());
}

TEST(ElfHandlerTest, AddSectionFromMissingFileFails)
{
    TempDir dir;
    auto elf = dir.path() / "copy";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());
    auto result = (*handler)->addSection(".ll-test", dir.path() / "missing");
    ASSERT_FALSE(result.has_value());
}

TEST(ElfHandlerTest, AddSectionFromEmptyFileFails)
{
    TempDir dir;
    auto elf = dir.path() / "copy";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    auto empty = dir.path() / "empty";
    std::ofstream emptyFile(empty);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());
    auto result = (*handler)->addSection(".ll-test", empty);
    ASSERT_FALSE(result.has_value());
}

TEST(ElfHandlerTest, AddSectionSucceedsOnRealElf)
{
    TempDir dir;
    auto elf = dir.path() / "copy";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "ll-extra-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // Reopen and verify the new section is actually present and valid in the modified ELF
    int checkFd = open(elf.c_str(), O_RDONLY);
    ASSERT_GE(checkFd, 0);
    elf_version(EV_CURRENT);
    Elf *checkElf = elf_begin(checkFd, ELF_C_READ, nullptr);
    ASSERT_NE(checkElf, nullptr);

    size_t checkShstrndx = 0;
    ASSERT_EQ(elf_getshdrstrndx(checkElf, &checkShstrndx), 0);

    bool foundSection = false;
    Elf_Scn *scn = nullptr;
    while ((scn = elf_nextscn(checkElf, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            continue;
        }
        const char *name = elf_strptr(checkElf, checkShstrndx, shdr.sh_name);
        if (name != nullptr && std::string(name) == ".ll-test") {
            foundSection = true;
            EXPECT_EQ(shdr.sh_size, payload.size());
            Elf_Data *data = elf_getdata(scn, nullptr);
            ASSERT_NE(data, nullptr);
            ASSERT_EQ(data->d_size, payload.size());
            EXPECT_EQ(std::string_view(static_cast<const char *>(data->d_buf), data->d_size),
                      std::string_view(payload));
            break;
        }
    }
    EXPECT_TRUE(foundSection);
    elf_end(checkElf);
    close(checkFd);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithZeroOrInvalidShstrndxAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-zero-shnum";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt the ELF header by zeroing e_shnum
    // For 64-bit ELF: e_shnum is at offset 60 (2 bytes)
    // For 32-bit ELF: e_shnum is at offset 48 (2 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shnumOffset = (ident[EI_CLASS] == ELFCLASS64) ? 60 : 48;
        uint16_t zeroShnum = 0;
        fs.seekp(shnumOffset);
        fs.write(reinterpret_cast<const char *>(&zeroShnum), sizeof(zeroShnum));
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("invalid section header count or string table index"));

    // Verify both file size and binary content remain completely untouched on pre-validation
    // failure
    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithUndefShstrndxAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-undef-shstrndx";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt e_shstrndx to SHN_UNDEF (0), indicating no section name string table
    // For 64-bit ELF: e_shstrndx is at offset 62 (2 bytes)
    // For 32-bit ELF: e_shstrndx is at offset 50 (2 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shstrndxOffset = (ident[EI_CLASS] == ELFCLASS64) ? 62 : 50;
        uint16_t undefShstrndx = SHN_UNDEF;
        fs.seekp(shstrndxOffset);
        fs.write(reinterpret_cast<const char *>(&undefShstrndx), sizeof(undefShstrndx));
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("invalid section header string table type"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithOutOfBoundsShstrndxAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-shstrndx";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt e_shstrndx to an out-of-bounds index (e.g. 0xFFFE)
    // For 64-bit ELF: e_shstrndx is at offset 62 (2 bytes)
    // For 32-bit ELF: e_shstrndx is at offset 50 (2 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shstrndxOffset = (ident[EI_CLASS] == ELFCLASS64) ? 62 : 50;
        uint16_t largeShstrndx = 0xFFFE;
        fs.seekp(shstrndxOffset);
        fs.write(reinterpret_cast<const char *>(&largeShstrndx), sizeof(largeShstrndx));
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("invalid section header count or string table index"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithNonStrtabShstrndxAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-shstrndx-type";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt the section header pointed to by e_shstrndx so its sh_type becomes SHT_PROGBITS (1)
    // instead of SHT_STRTAB (3).
    // In both 32-bit and 64-bit ELF, sh_type is a 4-byte word at offset 4 of the section header.
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);
        uint64_t shoff = 0;
        uint16_t shentsize = 0;
        uint16_t shstrndx = 0;

        if (is64) {
            fs.seekg(40);
            fs.read(reinterpret_cast<char *>(&shoff), sizeof(shoff));
            fs.seekg(58);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(62);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        } else {
            uint32_t shoff32 = 0;
            fs.seekg(32);
            fs.read(reinterpret_cast<char *>(&shoff32), sizeof(shoff32));
            shoff = shoff32;
            fs.seekg(46);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(50);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        }

        // sh_type is at shoff + shstrndx * shentsize + 4
        std::streamoff shTypeOffset = shoff + static_cast<uint64_t>(shstrndx) * shentsize + 4;
        uint32_t nonStrtabType = SHT_PROGBITS;
        fs.seekp(shTypeOffset);
        fs.write(reinterpret_cast<const char *>(&nonStrtabType), sizeof(nonStrtabType));
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("invalid section header string table type"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithOutOfBoundsStringTableOffsetAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-strtab-offset";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    const auto realFileSize = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt the section header string table's sh_offset to exceed file bounds.
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);
        uint64_t shoff = 0;
        uint16_t shentsize = 0;
        uint16_t shstrndx = 0;

        if (is64) {
            fs.seekg(40);
            fs.read(reinterpret_cast<char *>(&shoff), sizeof(shoff));
            fs.seekg(58);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(62);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        } else {
            uint32_t shoff32 = 0;
            fs.seekg(32);
            fs.read(reinterpret_cast<char *>(&shoff32), sizeof(shoff32));
            shoff = shoff32;
            fs.seekg(46);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(50);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        }

        ASSERT_GT(shentsize, 0U);
        ASSERT_GT(shstrndx, 0U);

        const std::streamoff strtabShdrOffset =
          shoff + static_cast<uint64_t>(shstrndx) * static_cast<uint64_t>(shentsize);

        if (is64) {
            // In Elf64_Shdr, sh_offset is at offset 24 (8 bytes)
            uint64_t outOfBoundsOffset = static_cast<uint64_t>(realFileSize) + 0x1000;
            fs.seekp(strtabShdrOffset + 24);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsOffset), sizeof(outOfBoundsOffset));
        } else {
            // In Elf32_Shdr, sh_offset is at offset 16 (4 bytes)
            uint32_t outOfBoundsOffset = static_cast<uint32_t>(realFileSize) + 0x1000;
            fs.seekp(strtabShdrOffset + 16);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsOffset), sizeof(outOfBoundsOffset));
        }
        ASSERT_TRUE(fs.good());
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("section header string table exceeds file bounds"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest,
     AddSectionRejectsElfWithPartiallyOutOfBoundsStringTableSizeAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-strtab-size";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    const auto realFileSize = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt the section header string table's sh_size such that sh_offset + sh_size
    // exceeds file bounds while sh_offset remains inside the file.
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);
        uint64_t shoff = 0;
        uint16_t shentsize = 0;
        uint16_t shstrndx = 0;

        if (is64) {
            fs.seekg(40);
            fs.read(reinterpret_cast<char *>(&shoff), sizeof(shoff));
            fs.seekg(58);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(62);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        } else {
            uint32_t shoff32 = 0;
            fs.seekg(32);
            fs.read(reinterpret_cast<char *>(&shoff32), sizeof(shoff32));
            shoff = shoff32;
            fs.seekg(46);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            fs.seekg(50);
            fs.read(reinterpret_cast<char *>(&shstrndx), sizeof(shstrndx));
        }

        ASSERT_GT(shentsize, 0U);
        ASSERT_GT(shstrndx, 0U);

        const std::streamoff strtabShdrOffset =
          shoff + static_cast<uint64_t>(shstrndx) * static_cast<uint64_t>(shentsize);

        if (is64) {
            // In Elf64_Shdr, sh_size is at offset 32 (8 bytes)
            uint64_t outOfBoundsSize = static_cast<uint64_t>(realFileSize) + 0x1000;
            fs.seekp(strtabShdrOffset + 32);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsSize), sizeof(outOfBoundsSize));
        } else {
            // In Elf32_Shdr, sh_size is at offset 20 (4 bytes)
            uint32_t outOfBoundsSize = static_cast<uint32_t>(realFileSize) + 0x1000;
            fs.seekp(strtabShdrOffset + 20);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsSize), sizeof(outOfBoundsSize));
        }
        ASSERT_TRUE(fs.good());
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("section header string table exceeds file bounds"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithInvalidSectionHeaderEntrySizeAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-shentsize";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt e_shentsize to an unexpected value (0x10)
    // For 64-bit ELF: e_shentsize is at offset 58 (2 bytes)
    // For 32-bit ELF: e_shentsize is at offset 46 (2 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        std::streamoff shentsizeOffset = (ident[EI_CLASS] == ELFCLASS64) ? 58 : 46;
        uint16_t invalidShentsize = 0x10;
        fs.seekp(shentsizeOffset);
        fs.write(reinterpret_cast<const char *>(&invalidShentsize), sizeof(invalidShentsize));
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("unsupported section header entry size"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithOutOfBoundsSectionHeaderTable)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-shoff";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    const auto realFileSize = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt e_shoff to an offset past the end of the file
    // For 64-bit ELF: e_shoff is at offset 40 (8 bytes)
    // For 32-bit ELF: e_shoff is at offset 32 (4 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        if (ident[EI_CLASS] == ELFCLASS64) {
            uint64_t outOfBoundsShoff = static_cast<uint64_t>(realFileSize) + 0x1000;
            fs.seekp(40);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsShoff), sizeof(outOfBoundsShoff));
        } else {
            uint32_t outOfBoundsShoff = static_cast<uint32_t>(realFileSize) + 0x1000;
            fs.seekp(32);
            fs.write(reinterpret_cast<const char *>(&outOfBoundsShoff), sizeof(outOfBoundsShoff));
        }
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("section header table exceeds file bounds"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest,
     AddSectionRejectsElfWithPartiallyOutOfBoundsSectionHeaderTableAndLeavesFileUntouched)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-partial-shoff";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    const auto realFileSize = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    ASSERT_GT(realFileSize, 64U);

    // Corrupt e_shoff to an offset inside the file, but where the remaining file size
    // is insufficient to hold the declared section header table (shnum * shentsize).
    // This specifically exercises the second bounds check (old_sht_size > fileSize - e_shoff)
    // rather than the first check (e_shoff > fileSize).
    // For 64-bit ELF: e_shoff is at offset 40 (8 bytes)
    // For 32-bit ELF: e_shoff is at offset 32 (4 bytes)
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        if (ident[EI_CLASS] == ELFCLASS64) {
            uint64_t partialShoff = static_cast<uint64_t>(realFileSize) - 16;
            fs.seekp(40);
            fs.write(reinterpret_cast<const char *>(&partialShoff), sizeof(partialShoff));
        } else {
            uint32_t partialShoff = static_cast<uint32_t>(realFileSize) - 16;
            fs.seekp(32);
            fs.write(reinterpret_cast<const char *>(&partialShoff), sizeof(partialShoff));
        }
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("section header table exceeds file bounds"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionRejectsElfWithExcessiveSectionHeaderCount)
{
    TempDir dir;
    auto elf = dir.path() / "corrupted-excessive-shnum";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    // Corrupt e_shnum to 0xFFFE (which exceeds supported non-extended limit 0xff00 - 1)
    // For 64-bit ELF: e_shnum is at offset 60 (2 bytes)
    // For 32-bit ELF: e_shnum is at offset 48 (2 bytes)
    uint64_t requiredSize = 0;
    {
        std::fstream fs(elf, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(fs.is_open());
        unsigned char ident[EI_NIDENT];
        fs.read(reinterpret_cast<char *>(ident), EI_NIDENT);
        ASSERT_EQ(fs.gcount(), EI_NIDENT);

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);
        uint64_t shoff = 0;
        uint16_t shentsize = 0;

        if (is64) {
            fs.seekg(40);
            fs.read(reinterpret_cast<char *>(&shoff), sizeof(shoff));
            fs.seekg(58);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            if (shentsize == 0) {
                shentsize = sizeof(Elf64_Shdr);
            }
        } else {
            uint32_t shoff32 = 0;
            fs.seekg(32);
            fs.read(reinterpret_cast<char *>(&shoff32), sizeof(shoff32));
            shoff = shoff32;
            fs.seekg(46);
            fs.read(reinterpret_cast<char *>(&shentsize), sizeof(shentsize));
            if (shentsize == 0) {
                shentsize = sizeof(Elf32_Shdr);
            }
        }

        std::streamoff shnumOffset = is64 ? 60 : 48;
        uint16_t excessiveShnum = 0xFFFE;
        fs.seekp(shnumOffset);
        fs.write(reinterpret_cast<const char *>(&excessiveShnum), sizeof(excessiveShnum));

        requiredSize =
          shoff + static_cast<uint64_t>(excessiveShnum) * static_cast<uint64_t>(shentsize);
    }

    // Expand the file to accommodate the declared section header table so that libelf's
    // elf_getshdrnum() validates the table within physical bounds and reports the excessive shnum
    // instead of reporting 0 due to truncation.
    const auto currentSize = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    if (requiredSize > currentSize) {
        std::filesystem::resize_file(elf, requiredSize, ec);
        ASSERT_FALSE(ec);
    }

    const auto sizeBefore = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    const auto contentBefore = readBinaryFile(elf);

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const std::string payload = "test-payload-data";
    auto result = (*handler)->addSection(".ll-test", payload.data(), payload.size());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("section header count exceeds supported limit"));

    const auto sizeAfter = std::filesystem::file_size(elf, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(sizeBefore, sizeAfter);

    const auto contentAfter = readBinaryFile(elf);
    EXPECT_EQ(contentBefore, contentAfter);
}

TEST(ElfHandlerTest, AddSectionFromFileCleansUpDescriptorsAndMapsOnSuccessAndFailure)
{
    if (!std::filesystem::exists("/proc/self/fd") || !std::filesystem::exists("/proc/self/maps")) {
        GTEST_SKIP() << "/proc/self not available";
    }

    TempDir dir;
    auto elf = dir.path() / "copy";
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", elf, ec);
    ASSERT_FALSE(ec);

    auto payloadFile = dir.path() / "payload.bin";
    {
        std::ofstream out(payloadFile, std::ios::binary);
        out << "payload-content-from-file";
    }

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    // 1. Success path: verify both descriptor and memory mapping are cleanly released
    auto result = (*handler)->addSection(".ll-file-sec", payloadFile);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(hasOpenFileDescriptorForPath(payloadFile));
    EXPECT_FALSE(hasMemoryMappingForPath(payloadFile));

    // 2. Failure path: verify both descriptor and mapping are cleanly released via RAII
    auto invalidElf = dir.path() / "not-elf-target";
    std::ofstream(invalidElf) << "plain-text-not-elf";
    auto invalidHandler = ElfHandler::create(invalidElf);
    ASSERT_TRUE(invalidHandler.has_value());

    auto failResult = (*invalidHandler)->addSection(".ll-fail-sec", payloadFile);
    ASSERT_FALSE(failResult.has_value());
    EXPECT_FALSE(hasOpenFileDescriptorForPath(payloadFile));
    EXPECT_FALSE(hasMemoryMappingForPath(payloadFile));
}

} // namespace
