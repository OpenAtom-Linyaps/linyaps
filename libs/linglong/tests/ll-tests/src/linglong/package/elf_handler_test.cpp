/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/elf_handler.h"

#include <common/tempdir.h>

#include <fstream>
#include <string>
#include <vector>

using namespace linglong::package;

namespace {

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
    EXPECT_FALSE(result.has_value());
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
    EXPECT_FALSE(result.has_value());
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
    EXPECT_FALSE(result.has_value());
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
    EXPECT_FALSE(result.has_value());
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
}

} // namespace

namespace {

std::filesystem::path copySelfExecutable(const TempDir &dir, const std::string &name)
{
    auto path = dir.path() / name;
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", path, ec);
    EXPECT_FALSE(ec) << ec.message();
    return path;
}

void setElfIdent(const std::filesystem::path &path, std::size_t offset, unsigned char value)
{
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.seekp(static_cast<std::streamoff>(offset));
    file.put(static_cast<char>(value));
    ASSERT_TRUE(file.good());
}

} // namespace

TEST(ElfHandlerTest, WriteSectionDataRewritesExistingSection)
{
    TempDir dir;
    auto elf = copySelfExecutable(dir, "copy");
    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->writeSectionData(".text", 0, payload, sizeof(payload));

    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST(ElfHandlerTest, WriteSectionDataRejectsUnknownSection)
{
    TempDir dir;
    auto elf = copySelfExecutable(dir, "copy");
    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->writeSectionData(".ll-missing-section", 0, payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("couldn't find section"));
}

TEST(ElfHandlerTest, WriteSectionDataRejectsOutOfRangeOffset)
{
    TempDir dir;
    auto elf = copySelfExecutable(dir, "copy");
    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->writeSectionData(".text", 1U << 30, payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("data exceeds section"));
}

TEST(ElfHandlerTest, WriteSectionDataReportsMissingFile)
{
    TempDir dir;
    auto handler = ElfHandler::create(dir.path() / "missing");
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->writeSectionData(".text", 0, payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("failed to open file"));
}

TEST(ElfHandlerTest, WriteSectionDataReportsNonElfFile)
{
    TempDir dir;
    auto plain = dir.path() / "plain";
    std::ofstream(plain) << "not an elf";
    auto handler = ElfHandler::create(plain);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->writeSectionData(".text", 0, payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("failed to get elf section header string index"));
}

TEST(ElfHandlerTest, AddSectionOnBigEndianHeadersDoesNotCrash)
{
    TempDir dir;
    auto elf = copySelfExecutable(dir, "copy");
    setElfIdent(elf, 5, 2); // EI_DATA = ELFDATA2MSB

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->addSection(".ll-msb", payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
}

TEST(ElfHandlerTest, AddSectionOn32BitBigEndianHeadersDoesNotCrash)
{
    TempDir dir;
    auto elf = copySelfExecutable(dir, "copy");
    setElfIdent(elf, 4, 1); // EI_CLASS = ELFCLASS32
    setElfIdent(elf, 5, 2); // EI_DATA = ELFDATA2MSB

    auto handler = ElfHandler::create(elf);
    ASSERT_TRUE(handler.has_value());

    const char payload[] = { 'A' };
    auto result = (*handler)->addSection(".ll-32msb", payload, sizeof(payload));

    EXPECT_FALSE(result.has_value());
}
