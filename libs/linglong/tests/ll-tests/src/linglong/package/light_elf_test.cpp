// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "light_elf.h"

#include <elf.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// -----------------------------------------------------------------------------
// Synthetic ELF images
//
// The tests below hand-assemble minimal ELF64 files instead of relying on a real
// toolchain: the parser under test only needs a valid ELF header, a section header
// string table and a section header table. Every field the parser reads is placed
// explicitly so that the happy path and the malformed variants stay readable.
// -----------------------------------------------------------------------------

constexpr std::size_t kElfHeaderSize = sizeof(Elf64_Ehdr);
constexpr std::size_t kSectionHeaderSize = sizeof(Elf64_Shdr);

// Section header string table used by the default image:
//   [0]      ""              (the empty name referenced by the null section)
//   [1..9]   ".shstrtab"
//   [11..24] ".linglong.meta"
constexpr std::uint32_t kShstrtabNameOffset = 1;
constexpr std::uint32_t kMetaNameOffset = 11;
const std::string kStrTab = std::string("\0.shstrtab\0.linglong.meta", 26);

constexpr std::uint64_t kStrTabFileOffset = kElfHeaderSize;

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

// Every knob a test needs to build a valid or deliberately malformed image.
struct ImageOptions
{
    std::uint16_t sectionHeaderEntrySize{ static_cast<std::uint16_t>(kSectionHeaderSize) };
    std::uint16_t sectionCount{ 3 };
    std::uint16_t stringTableIndex{ 1 };
    bool useExtendedStringTableIndex{ false };
    std::uint32_t stringTableType{ SHT_STRTAB };
    std::uint64_t stringTableFileOffset{ kStrTabFileOffset };
    std::optional<std::uint64_t> stringTableShSizeOverride{};
    std::uint32_t metaNameOffset{ kMetaNameOffset };
    bool skipSectionTable{ false };
    std::string stringTableData{ kStrTab };
    std::optional<std::size_t> truncateTo{};
};

std::vector<char> buildImage(const ImageOptions &opts)
{
    const std::uint64_t strTabOffset = kStrTabFileOffset;
    const std::uint64_t strTabSize = opts.stringTableData.size();
    const std::uint64_t sectionTableOffset = alignUp(strTabOffset + strTabSize, 8);
    const std::uint64_t sectionCount = opts.sectionCount;
    // Always reserve room for at least the null section, even when e_shnum is 0.
    const std::uint64_t reservedEntries = std::max<std::uint64_t>(sectionCount, 1);
    // Per the ELF ABI the table stride is e_shentsize, so a hostile value is laid out
    // exactly the way the buggy parser would trust it. The entries themselves stay 64
    // bytes; only the spacing between them follows e_shentsize.
    const std::uint64_t sectionEntrySize = opts.sectionHeaderEntrySize;
    // The last entry still needs a full 64 byte structure even when the (hostile)
    // stride is smaller than that.
    const std::uint64_t sectionTableSpan =
      (reservedEntries - 1) * sectionEntrySize + kSectionHeaderSize;
    const std::uint64_t sectionTableSize =
      std::max<std::uint64_t>(reservedEntries * sectionEntrySize, sectionTableSpan);

    std::vector<char> image(static_cast<std::size_t>(sectionTableOffset + sectionTableSize), '\0');

    Elf64_Ehdr elfHeader{};
    elfHeader.e_ident[EI_MAG0] = ELFMAG0;
    elfHeader.e_ident[EI_MAG1] = ELFMAG1;
    elfHeader.e_ident[EI_MAG2] = ELFMAG2;
    elfHeader.e_ident[EI_MAG3] = ELFMAG3;
    elfHeader.e_ident[EI_CLASS] = ELFCLASS64;
    elfHeader.e_ident[EI_DATA] = ELFDATA2LSB;
    elfHeader.e_ident[EI_VERSION] = EV_CURRENT;
    elfHeader.e_type = ET_REL;
    elfHeader.e_machine = EM_X86_64;
    elfHeader.e_version = EV_CURRENT;
    elfHeader.e_shoff = opts.skipSectionTable ? 0 : sectionTableOffset;
    elfHeader.e_ehsize = static_cast<Elf64_Half>(kElfHeaderSize);
    elfHeader.e_shentsize = opts.sectionHeaderEntrySize;
    elfHeader.e_shnum = static_cast<Elf64_Half>(sectionCount);
    elfHeader.e_shstrndx = opts.useExtendedStringTableIndex ? SHN_XINDEX : opts.stringTableIndex;
    std::memcpy(image.data(), &elfHeader, sizeof(elfHeader));

    std::memcpy(image.data() + strTabOffset, opts.stringTableData.data(), strTabSize);

    const auto writeSectionHeader = [&](std::uint64_t index, const Elf64_Shdr &shdr) {
        std::memcpy(image.data() + sectionTableOffset + index * sectionEntrySize,
                    &shdr,
                    sizeof(shdr));
    };

    // Section 0 is the mandatory null entry. When e_shstrndx is SHN_XINDEX the real
    // string table index lives in its sh_link field.
    if (opts.useExtendedStringTableIndex) {
        Elf64_Shdr nullSection{};
        nullSection.sh_link = opts.stringTableIndex;
        writeSectionHeader(0, nullSection);
    }

    if (sectionCount > 1) {
        Elf64_Shdr stringTableSection{};
        stringTableSection.sh_name = kShstrtabNameOffset;
        stringTableSection.sh_type = opts.stringTableType;
        stringTableSection.sh_offset = opts.stringTableFileOffset;
        stringTableSection.sh_size = opts.stringTableShSizeOverride.value_or(strTabSize);
        writeSectionHeader(1, stringTableSection);
    }

    if (sectionCount > 2) {
        Elf64_Shdr metaSection{};
        metaSection.sh_name = opts.metaNameOffset;
        metaSection.sh_type = SHT_PROGBITS;
        writeSectionHeader(2, metaSection);
    }

    if (opts.truncateTo.has_value() && *opts.truncateTo < image.size()) {
        image.resize(*opts.truncateTo);
    }

    return image;
}

class LightElfTest : public ::testing::Test
{
protected:
    void SetUp() override { workspace = std::make_unique<TempDir>("linglong-light-elf-test-"); }

    [[nodiscard]] std::filesystem::path writeImage(const std::vector<char> &image,
                                                   const std::string &name = "sample.elf") const
    {
        const auto path = workspace->path() / name;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        EXPECT_TRUE(output.is_open()) << "failed to create " << path;
        output.write(image.data(), static_cast<std::streamsize>(image.size()));
        output.close();
        return path;
    }

    std::unique_ptr<TempDir> workspace;
};

// A parse failure must be reported as an exception raised by the constructor instead
// of corrupting memory or silently reading garbage.
void expectConstructorRejected(const std::filesystem::path &path)
{
    EXPECT_THROW({ lightElf::native_elf elf(path); }, std::runtime_error);
}

} // namespace

// The happy path must keep resolving section names through the section header string
// table, including a repeated lookup (the second call must be a safe no-op).
TEST_F(LightElfTest, ResolvesSectionByName)
{
    const auto path = writeImage(buildImage({}));

    lightElf::native_elf elf(path);
    EXPECT_EQ(elf.fileHeader().e_shnum, 3);

    const auto meta = elf.getSectionHeader(".linglong.meta");
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->sh_type, SHT_PROGBITS);
    EXPECT_EQ(meta->sh_name, kMetaNameOffset);

    const auto strtab = elf.getSectionHeader(".shstrtab");
    ASSERT_TRUE(strtab.has_value());
    EXPECT_EQ(strtab->sh_type, SHT_STRTAB);

    // Unknown and empty names must not match any section.
    EXPECT_FALSE(elf.getSectionHeader("does.not.exist").has_value());
    EXPECT_FALSE(elf.getSectionHeader(".linglong.meta.extra").has_value());
    EXPECT_FALSE(elf.getSectionHeader("").has_value());

    const auto repeated = elf.getSectionHeader(".linglong.meta");
    ASSERT_TRUE(repeated.has_value());
    EXPECT_EQ(repeated->sh_offset, meta->sh_offset);
    EXPECT_EQ(repeated->sh_size, meta->sh_size);
}

TEST_F(LightElfTest, RejectsSectionHeaderEntrySizeLargerThanSectionHeader)
{
    ImageOptions opts;
    opts.sectionHeaderEntrySize = 0x0100; // larger than sizeof(Elf64_Shdr) == 64
    const auto path = writeImage(buildImage(opts));

    // The table is laid out with the hostile stride, so without the guard the parser
    // accepts the file and getSectionHeader() later reads 0x0100 bytes into a 64 byte
    // stack object (a stack-buffer-overflow reported by ASan). The constructor must
    // reject the file before any of that happens.
    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsSectionHeaderEntrySizeSmallerThanSectionHeader)
{
    ImageOptions opts;
    opts.sectionHeaderEntrySize = 32; // smaller than sizeof(Elf64_Shdr) == 64
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsEmptySectionHeaderTable)
{
    ImageOptions opts;
    opts.sectionCount = 0;
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsSectionHeaderTableOutsideOfFile)
{
    ImageOptions opts;
    const auto full = buildImage(opts);
    // Cut the file in the middle of the section header table.
    opts.truncateTo = full.size() - kSectionHeaderSize / 2;
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsStringTableOutsideOfFile)
{
    ImageOptions opts;
    opts.stringTableShSizeOverride = 0x100000; // far beyond the end of the file
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsStringTableIndexOutOfRange)
{
    ImageOptions opts;
    opts.stringTableIndex = 5; // only three sections exist
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsStringTableWithWrongType)
{
    ImageOptions opts;
    opts.stringTableType = SHT_PROGBITS;
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsMissingSectionHeaderTable)
{
    ImageOptions opts;
    opts.skipSectionTable = true; // e_shoff == 0
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsMissingStringTableIndex)
{
    ImageOptions opts;
    opts.stringTableIndex = SHN_UNDEF;
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

TEST_F(LightElfTest, RejectsTruncatedElfHeader)
{
    ImageOptions opts;
    opts.truncateTo = 10; // shorter than sizeof(Elf64_Ehdr)
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}

// sh_name is a 32 bit offset into the string table, so a crafted value must be skipped
// instead of building a string_view past the end of the heap buffer. Without the bound
// this test trips a heap-buffer-overflow under ASan.
TEST_F(LightElfTest, SkipsSectionNameOutsideOfStringTable)
{
    ImageOptions opts;
    opts.metaNameOffset = 0x7fffffff;
    const auto path = writeImage(buildImage(opts));

    lightElf::native_elf elf(path);

    EXPECT_FALSE(elf.getSectionHeader(".linglong.meta").has_value());
    EXPECT_FALSE(elf.getSectionHeader("does.not.exist").has_value());
    // The remaining, well formed entry must still resolve.
    EXPECT_TRUE(elf.getSectionHeader(".shstrtab").has_value());
}

// A string table whose last name is not NUL terminated must still yield a bounded view:
// the search stops at the end of the buffer instead of reading past it.
TEST_F(LightElfTest, BoundsNameWithoutTerminatingNul)
{
    ImageOptions opts;
    opts.stringTableData = std::string("\0.shstrtab\0.linglong.meta", 25);
    const auto path = writeImage(buildImage(opts));

    lightElf::native_elf elf(path);

    const auto meta = elf.getSectionHeader(".linglong.meta");
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->sh_type, SHT_PROGBITS);
    EXPECT_FALSE(elf.getSectionHeader(".linglong.meta.extra").has_value());
}

// e_shstrndx == SHN_XINDEX moves the real index into sh_link of section 0.
TEST_F(LightElfTest, ResolvesExtendedStringTableIndex)
{
    ImageOptions opts;
    opts.useExtendedStringTableIndex = true;
    const auto path = writeImage(buildImage(opts));

    lightElf::native_elf elf(path);

    const auto meta = elf.getSectionHeader(".linglong.meta");
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->sh_type, SHT_PROGBITS);
}

TEST_F(LightElfTest, RejectsExtendedStringTableIndexOutOfRange)
{
    ImageOptions opts;
    opts.useExtendedStringTableIndex = true;
    opts.stringTableIndex = 5; // stored in sh_link of the null section
    const auto path = writeImage(buildImage(opts));

    expectConstructorRejected(path);
}
