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
