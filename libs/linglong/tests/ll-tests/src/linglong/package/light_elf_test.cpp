// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "light_elf.h"

#include <common/tempdir.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using NativeElf = lightElf::native_elf;
using ElfHeader = NativeElf::ElfHeader;
using SecHeader = NativeElf::SectionHeader;

// Fixed layout of the synthetic ELF image built by makeSyntheticElf().  The offsets are
// deliberately simple so that a test can patch one field of the image without having to
// reimplement the whole builder.
constexpr std::size_t kStrTabOffset = 0x100;
constexpr std::size_t kSectionTableOffset = 0x200;
constexpr std::uint16_t kTextIndex = 1;
constexpr std::uint16_t kDataIndex = 2;
constexpr std::uint16_t kStrTabIndex = 3;
constexpr std::uint16_t kSectionCount = 4;

std::vector<char> readWholeFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeWholeFile(const std::filesystem::path &path, const std::vector<char> &bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <typename T>
void storeAt(std::vector<char> &bytes, std::size_t offset, const T &value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

template <typename T>
T loadAt(const std::vector<char> &bytes, std::size_t offset)
{
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

// A tiny, fully controlled ELF image: a null section, a .text and a .data section plus a
// section-name string table.  Because every byte is produced here, a test can model the
// malformed headers an attacker would put into a .uab, which a real linker would never
// emit.  `entrySize` is written to e_shentsize and also used as the stride of the table
// so that the reader can still locate the string table before the entry-size check runs.
struct SyntheticElf
{
    std::vector<char> bytes;
    std::size_t entrySize{ sizeof(SecHeader) };
    std::size_t textNameOffset{};
    std::size_t dataNameOffset{};
    std::size_t strTabNameOffset{};
    std::size_t strTabSize{};

    std::size_t sectionOffset(std::uint16_t index) const
    {
        return kSectionTableOffset + static_cast<std::size_t>(index) * entrySize;
    }

    ElfHeader header() const { return loadAt<ElfHeader>(bytes, 0); }

    void setHeader(const ElfHeader &value) { storeAt(bytes, 0, value); }

    SecHeader section(std::uint16_t index) const
    {
        return loadAt<SecHeader>(bytes, sectionOffset(index));
    }

    void setSection(std::uint16_t index, const SecHeader &value)
    {
        storeAt(bytes, sectionOffset(index), value);
    }

    void writeTo(const std::filesystem::path &path) const { writeWholeFile(path, bytes); }
};

SyntheticElf makeSyntheticElf(std::size_t entrySize = sizeof(SecHeader),
                              std::uint16_t sectionCount = kSectionCount)
{
    SyntheticElf image;
    image.entrySize = entrySize;

    // The table starts with a NUL so that sh_name == 0 is the empty name of the null
    // section; the names used by the tests are appended afterwards.
    std::string strTab(1, '\0');
    image.textNameOffset = strTab.size();
    strTab.append(".text").push_back('\0');
    image.dataNameOffset = strTab.size();
    strTab.append(".data").push_back('\0');
    image.strTabNameOffset = strTab.size();
    strTab.append(".shstrtab").push_back('\0');
    image.strTabSize = strTab.size();

    // The table has to be large enough for the last entry as it is written below, and
    // for the string table regardless of the stride used by a tampered e_shentsize.
    const auto lastEntryEnd = kSectionTableOffset
      + static_cast<std::size_t>(sectionCount - 1) * entrySize + sizeof(SecHeader);
    const auto minSize = std::max(lastEntryEnd, kStrTabOffset + strTab.size());
    image.bytes.assign(minSize, '\0');
    std::memcpy(image.bytes.data() + kStrTabOffset, strTab.data(), strTab.size());

    ElfHeader header{};
    header.e_ident[EI_MAG0] = ELFMAG0;
    header.e_ident[EI_MAG1] = ELFMAG1;
    header.e_ident[EI_MAG2] = ELFMAG2;
    header.e_ident[EI_MAG3] = ELFMAG3;
    header.e_ident[EI_CLASS] = lightElf::machine_type == 64 ? ELFCLASS64 : ELFCLASS32;
    header.e_shoff = kSectionTableOffset;
    header.e_shentsize = static_cast<std::uint16_t>(entrySize);
    header.e_shnum = sectionCount;
    header.e_shstrndx = kStrTabIndex;
    image.setHeader(header);

    SecHeader text{};
    text.sh_name = static_cast<decltype(text.sh_name)>(image.textNameOffset);
    text.sh_type = SHT_PROGBITS;
    text.sh_offset = kStrTabOffset;
    text.sh_size = 4;
    image.setSection(kTextIndex, text);

    SecHeader data = text;
    data.sh_name = static_cast<decltype(data.sh_name)>(image.dataNameOffset);
    image.setSection(kDataIndex, data);

    SecHeader strTabSection{};
    strTabSection.sh_name = static_cast<decltype(strTabSection.sh_name)>(image.strTabNameOffset);
    strTabSection.sh_type = SHT_STRTAB;
    strTabSection.sh_offset = kStrTabOffset;
    strTabSection.sh_size = strTab.size();
    image.setSection(kStrTabIndex, strTabSection);

    return image;
}

// A well-formed image must still be parsed completely: the constructor accepts it and
// getSectionHeader() finds every section by the name stored in the string table.
TEST(LightElfTest, EnumeratesEverySectionOfTheRunningExecutable)
{
    TempDir dir;
    const auto path = dir.path() / "self.elf";

    // /proc/self/exe is a real linker-produced ELF, so the section count, the entry size
    // and every name come from an actual toolchain instead of from this test.
    std::error_code ec;
    std::filesystem::copy_file("/proc/self/exe", path, ec);
    ASSERT_FALSE(ec);

    lightElf::native_elf elf(path);

    const auto raw = readWholeFile(path);
    const auto header = loadAt<ElfHeader>(raw, 0);
    ASSERT_EQ(header.e_shentsize, sizeof(SecHeader));
    ASSERT_GT(header.e_shnum, 1);

    const auto strTabSection = loadAt<SecHeader>(
      raw,
      header.e_shoff + static_cast<std::size_t>(header.e_shstrndx) * header.e_shentsize);
    const std::string strTab(raw.data() + strTabSection.sh_offset, strTabSection.sh_size);

    std::size_t matched = 0;
    for (decltype(header.e_shnum) index = 1; index < header.e_shnum; ++index) {
        const auto shdr =
          loadAt<SecHeader>(raw,
                            header.e_shoff + static_cast<std::size_t>(index) * header.e_shentsize);
        if (shdr.sh_name == 0 || shdr.sh_name >= strTab.size()) {
            continue;
        }

        const std::string_view name(strTab.data() + shdr.sh_name);
        if (name.empty()) {
            continue;
        }

        const auto found = elf.getSectionHeader(std::string{ name });
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->sh_offset, shdr.sh_offset);
        EXPECT_EQ(found->sh_size, shdr.sh_size);
        EXPECT_EQ(found->sh_type, shdr.sh_type);
        ++matched;
    }

    EXPECT_GT(matched, 0u);
}

TEST(LightElfTest, FindsSectionInSyntheticImage)
{
    TempDir dir;
    const auto path = dir.path() / "synthetic.elf";
    makeSyntheticElf().writeTo(path);

    lightElf::native_elf elf(path);

    ASSERT_TRUE(elf.getSectionHeader(".text").has_value());
    ASSERT_TRUE(elf.getSectionHeader(".data").has_value());
    ASSERT_TRUE(elf.getSectionHeader(".shstrtab").has_value());
    EXPECT_FALSE(elf.getSectionHeader(".does-not-exist").has_value());
}

// e_shentsize is a 16 bit header field taken from the file.  A value larger than
// sizeof(SectionHeader) used to make getSectionHeader() read that many bytes into the
// fixed on-stack SectionHeader, i.e. a stack buffer overflow.  Such a file must now be
// rejected with an empty result instead of being read.
TEST(LightElfTest, RejectsSectionHeaderEntrySizeLargerThanStruct)
{
    TempDir dir;
    const auto path = dir.path() / "oversized-entry.elf";
    makeSyntheticElf(2 * sizeof(SecHeader)).writeTo(path);

    lightElf::native_elf elf(path);
    ASSERT_NE(elf.fileHeader().e_shentsize, sizeof(SecHeader));

    EXPECT_FALSE(elf.getSectionHeader(".text").has_value());
    EXPECT_FALSE(elf.getSectionHeader(".shstrtab").has_value());
}

// A stride smaller than sizeof(SectionHeader) leaves the on-stack header only partially
// filled, so every field that is not covered by the read stays uninitialised and feeds
// attacker-controlled offsets into the rest of the loop.  It must be rejected as well.
TEST(LightElfTest, RejectsSectionHeaderEntrySizeSmallerThanStruct)
{
    TempDir dir;
    const auto path = dir.path() / "undersized-entry.elf";
    makeSyntheticElf(sizeof(SecHeader) / 2).writeTo(path);

    lightElf::native_elf elf(path);
    ASSERT_NE(elf.fileHeader().e_shentsize, sizeof(SecHeader));

    EXPECT_FALSE(elf.getSectionHeader(".text").has_value());
    EXPECT_FALSE(elf.getSectionHeader(".shstrtab").has_value());
}

// e_shnum is also read from the file.  Claiming far more entries than the file can hold
// must not make the loop walk (and read) past the end of the section header table.
TEST(LightElfTest, RejectsSectionTablePastEndOfFile)
{
    TempDir dir;
    const auto path = dir.path() / "truncated-table.elf";

    auto image = makeSyntheticElf();
    auto header = image.header();
    header.e_shnum = 0xFFFF;
    image.setHeader(header);
    image.writeTo(path);

    lightElf::native_elf elf(path);
    EXPECT_FALSE(elf.getSectionHeader(".text").has_value());
    EXPECT_FALSE(elf.getSectionHeader(".shstrtab").has_value());
}

// sh_name is an offset into the string table.  An offset outside the table used to build
// a string_view that scanned memory far beyond rawSectionNames, so the check below has
// to happen before the view is created.  The selected offset is well beyond any heap
// mapping, which makes the missing check crash the old code even without a sanitizer.
TEST(LightElfTest, RejectsOutOfRangeSectionNameOffset)
{
    TempDir dir;
    const auto path = dir.path() / "bad-name-offset.elf";

    auto image = makeSyntheticElf();
    auto corrupt = image.section(kDataIndex);
    corrupt.sh_name = static_cast<decltype(corrupt.sh_name)>(0xFFFFFFF0u);
    image.setSection(kDataIndex, corrupt);
    image.writeTo(path);

    lightElf::native_elf elf(path);

    // The corrupt entry can never be matched ...
    EXPECT_FALSE(elf.getSectionHeader(".data").has_value());
    // ... but it must not hide the entries that follow it in the table.
    EXPECT_TRUE(elf.getSectionHeader(".text").has_value());
    EXPECT_TRUE(elf.getSectionHeader(".shstrtab").has_value());
}

// A section name whose terminator is missing (a truncated string table) must be skipped
// rather than turned into a view that reads past the end of the buffer.
TEST(LightElfTest, RejectsUnterminatedSectionName)
{
    TempDir dir;
    const auto path = dir.path() / "unterminated-name.elf";

    auto image = makeSyntheticElf();
    ASSERT_EQ(image.bytes[kStrTabOffset + image.strTabSize - 1], '\0');

    // Replace the final NUL with a regular byte: the string table still has exactly the
    // declared size and stays inside the file, but the last name is now unterminated.
    image.bytes[kStrTabOffset + image.strTabSize - 1] = 'x';
    image.writeTo(path);

    lightElf::native_elf elf(path);

    EXPECT_FALSE(elf.getSectionHeader(".shstrtab").has_value());
    // Names that are properly terminated earlier in the table keep working.
    EXPECT_TRUE(elf.getSectionHeader(".text").has_value());
    EXPECT_TRUE(elf.getSectionHeader(".data").has_value());
}

// Repeated lookups on the same object must stay safe and stable: the validation above
// must not depend on, or mutate, state left behind by a previous call.
TEST(LightElfTest, RepeatedLookupsAreStable)
{
    TempDir dir;
    const auto path = dir.path() / "repeated.elf";

    auto image = makeSyntheticElf();
    auto corrupt = image.section(kDataIndex);
    corrupt.sh_name = static_cast<decltype(corrupt.sh_name)>(0xFFFFFFF0u);
    image.setSection(kDataIndex, corrupt);
    image.writeTo(path);

    lightElf::native_elf elf(path);

    for (auto i = 0; i < 2; ++i) {
        EXPECT_FALSE(elf.getSectionHeader(".data").has_value());
        EXPECT_TRUE(elf.getSectionHeader(".text").has_value());
    }
}

} // namespace
