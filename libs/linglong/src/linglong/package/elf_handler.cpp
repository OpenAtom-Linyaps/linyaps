// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "elf_handler.h"

#include "linglong/utils/finally/finally.h"

#include <byteswap.h>
#include <endian.h>
#include <gelf.h>
#include <libelf.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

bool is_host_lsb()
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return true;
#else
    return false;
#endif
}

template <typename T>
T swap_val(T val)
{
    return val;
}

template <>
uint16_t swap_val(uint16_t val)
{
    return bswap_16(val);
}

template <>
uint32_t swap_val(uint32_t val)
{
    return bswap_32(val);
}

template <>
uint64_t swap_val(uint64_t val)
{
    return bswap_64(val);
}

struct EndianSwapper
{
    bool need_swap;

    EndianSwapper(bool is_file_lsb) { need_swap = (is_file_lsb != is_host_lsb()); }

    template <typename T>
    T operator()(T val) const
    {
        return need_swap ? swap_val(val) : val;
    }

    template <typename EhdrT>
    void swap_header(EhdrT &hdr) const
    {
        if (!need_swap)
            return;
        hdr.e_type = swap_val(hdr.e_type);
        hdr.e_machine = swap_val(hdr.e_machine);
        hdr.e_version = swap_val(hdr.e_version);
        hdr.e_entry = swap_val(hdr.e_entry);
        hdr.e_phoff = swap_val(hdr.e_phoff);
        hdr.e_shoff = swap_val(hdr.e_shoff);
        hdr.e_flags = swap_val(hdr.e_flags);
        hdr.e_ehsize = swap_val(hdr.e_ehsize);
        hdr.e_phentsize = swap_val(hdr.e_phentsize);
        hdr.e_phnum = swap_val(hdr.e_phnum);
        hdr.e_shentsize = swap_val(hdr.e_shentsize);
        hdr.e_shnum = swap_val(hdr.e_shnum);
        hdr.e_shstrndx = swap_val(hdr.e_shstrndx);
    }

    template <typename ShdrT>
    void swap_shdr(ShdrT &sh) const
    {
        if (!need_swap)
            return;
        sh.sh_name = swap_val(sh.sh_name);
        sh.sh_type = swap_val(sh.sh_type);
        sh.sh_flags = swap_val(sh.sh_flags);
        sh.sh_addr = swap_val(sh.sh_addr);
        sh.sh_offset = swap_val(sh.sh_offset);
        sh.sh_size = swap_val(sh.sh_size);
        sh.sh_link = swap_val(sh.sh_link);
        sh.sh_info = swap_val(sh.sh_info);
        sh.sh_addralign = swap_val(sh.sh_addralign);
        sh.sh_entsize = swap_val(sh.sh_entsize);
    }
};

bool write_all(int fd, const void *buf, size_t count)
{
    const char *ptr = static_cast<const char *>(buf);
    size_t left = count;
    while (left > 0) {
        ssize_t written = write(fd, ptr, left);
        if (written <= 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        ptr += written;
        left -= written;
    }
    return true;
}

off_t align_file(int fd, size_t alignment)
{
    off_t current = lseek(fd, 0, SEEK_END);
    if (current == -1) {
        return -1;
    }
    if (current % alignment == 0) {
        return current;
    }

    size_t pad_len = alignment - (current % alignment);
    const char zeros[16] = { 0 };
    size_t written = 0;
    while (written < pad_len) {
        size_t to_write = std::min(pad_len - written, sizeof(zeros));
        if (!write_all(fd, zeros, to_write)) {
            return -1;
        }
        written += to_write;
    }

    return lseek(fd, 0, SEEK_END);
}

template <typename EhdrT, typename ShdrT>
linglong::utils::error::Result<void>
addSectionImpl(int fd, Elf *e, const std::string &name, const char *data, size_t size, bool is_lsb)
{
    LINGLONG_TRACE("add section impl");

    EndianSwapper swapper(is_lsb);

    auto appended_offset = align_file(fd, 16);
    if (appended_offset == -1) {
        return LINGLONG_ERR("failed to align file");
    }

    if (!write_all(fd, data, size)) {
        return LINGLONG_ERR("failed to append section data");
    }

    EhdrT ehdr;
    if (pread(fd, &ehdr, sizeof(ehdr), 0) != sizeof(ehdr)) {
        return LINGLONG_ERR("failed to read elf header");
    }
    swapper.swap_header(ehdr);

    size_t shstrndx;
    if (elf_getshdrstrndx(e, &shstrndx) != 0) {
        return LINGLONG_ERR("failed to get elf section header string index");
    }

    Elf_Scn *scn_str = elf_getscn(e, shstrndx);
    if (scn_str == nullptr) {
        return LINGLONG_ERR("failed to get elf section header string");
    }
    GElf_Shdr shdr_str;
    if (gelf_getshdr(scn_str, &shdr_str) != &shdr_str) {
        return LINGLONG_ERR("failed to get strtab header");
    }
    if (shdr_str.sh_flags & SHF_COMPRESSED) {
        return LINGLONG_ERR("compressed string table not supported");
    }

    Elf_Data *data_str = elf_getdata(scn_str, nullptr);
    if (data_str == nullptr) {
        return LINGLONG_ERR("failed to get elf section header string data");
    }

    size_t old_strtable_size = data_str->d_size;
    size_t new_name_len = name.length() + 1;
    size_t new_strtable_size = old_strtable_size + new_name_len;

    auto new_strtab_offset = align_file(fd, 1);
    if (new_strtab_offset == -1) {
        return LINGLONG_ERR("failed to align file");
    }

    if (!write_all(fd, data_str->d_buf, old_strtable_size)) {
        return LINGLONG_ERR("failed to write old strtab");
    }

    if (!write_all(fd, name.c_str(), new_name_len)) {
        return LINGLONG_ERR("failed to write new strtab");
    }

    size_t new_name_idx = old_strtable_size;

    size_t shnum = 0;
    elf_getshdrnum(e, &shnum);

    size_t old_sht_size = shnum * sizeof(ShdrT);
    auto sht_buf = std::make_unique<ShdrT[]>(shnum);
    size_t read = pread(fd, sht_buf.get(), old_sht_size, ehdr.e_shoff);
    if (read != old_sht_size) {
        return LINGLONG_ERR("failed to read sht");
    }

    for (size_t i = 0; i < shnum; ++i) {
        swapper.swap_shdr(sht_buf[i]);
    }
    sht_buf[shstrndx].sh_offset = new_strtab_offset;
    sht_buf[shstrndx].sh_size = new_strtable_size;

    size_t sht_align = sizeof(typename std::conditional < sizeof(ShdrT) == sizeof(Elf64_Shdr),
                              uint64_t,
                              uint32_t > ::type);
    auto new_sht_offset = align_file(fd, sht_align);
    if (new_sht_offset == -1) {
        return LINGLONG_ERR("failed to align file");
    }
    for (size_t i = 0; i < shnum; ++i) {
        swapper.swap_shdr(sht_buf[i]);
    }
    if (!write_all(fd, sht_buf.get(), old_sht_size)) {
        return LINGLONG_ERR("failed to write sht");
    }

    ShdrT new_shdr;
    memset(&new_shdr, 0, sizeof(new_shdr));

    new_shdr.sh_name = new_name_idx;
    new_shdr.sh_type = SHT_PROGBITS;
    new_shdr.sh_flags = 0;
    new_shdr.sh_addr = 0;
    new_shdr.sh_offset = appended_offset;
    new_shdr.sh_size = size;
    new_shdr.sh_link = 0;
    new_shdr.sh_info = 0;
    new_shdr.sh_addralign = 1;
    new_shdr.sh_entsize = 0;

    swapper.swap_shdr(new_shdr);
    if (!write_all(fd, &new_shdr, sizeof(new_shdr))) {
        return LINGLONG_ERR("failed to write new shdr");
    }

    ehdr.e_shoff = new_sht_offset;
    ehdr.e_shnum = shnum + 1;
    swapper.swap_header(ehdr);
    lseek(fd, 0, SEEK_SET);
    if (!write_all(fd, &ehdr, sizeof(ehdr))) {
        return LINGLONG_ERR("failed to write new ehdr");
    }

    return LINGLONG_OK;
}

} // namespace

namespace linglong::package {

utils::error::Result<std::unique_ptr<ElfHandler>> ElfHandler::create(std::filesystem::path file)
{
    LINGLONG_TRACE("create elfhandler: " + QString::fromStdString(file.string()));

    return std::make_unique<ElfHandler>(file);
}

ElfHandler::ElfHandler(std::filesystem::path file)
    : file_(std::move(file))
{
}

ElfHandler::~ElfHandler() { }

utils::error::Result<void> ElfHandler::addSection(const std::string &name,
                                                  const std::filesystem::path &file)
{
    LINGLONG_TRACE("add section from file: " + QString::fromStdString(file.string()));

    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        return LINGLONG_ERR("failed to get file size for '" + QString::fromStdString(file.string())
                            + "': " + QString::fromStdString(ec.message()));
    }

    if (size == 0) {
        return LINGLONG_ERR("file is empty: " + QString::fromStdString(file.string()));
    }

    int fd = open(file.c_str(), O_RDONLY);
    if (fd < 0) {
        return LINGLONG_ERR("failed to open file: " + QString::fromStdString(file.string()));
    }

    void *data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (data == MAP_FAILED) {
        return LINGLONG_ERR("failed to mmap file: " + QString::fromStdString(file.string()));
    }

    auto result = addSection(name, static_cast<const char *>(data), size);

    munmap(data, size);
    close(fd);

    return result;
}

utils::error::Result<void> ElfHandler::addSection(const std::string &name,
                                                  const char *data,
                                                  size_t size)
{
    LINGLONG_TRACE("add section:" + QString::fromStdString(name));

    if (name.length() > 16) {
        return LINGLONG_ERR("section name too long");
    }

    if (elf_version(EV_CURRENT) == EV_NONE) {
        return LINGLONG_ERR("failed to get elf version");
    }

    int fd = open(file_.c_str(), O_RDWR);
    if (fd < 0) {
        return LINGLONG_ERR("failed to open file: " + QString::fromStdString(file_.string()));
    }
    auto close_fd = utils::finally::finally([fd] {
        close(fd);
    });

    Elf *e = elf_begin(fd, ELF_C_READ_MMAP, nullptr);
    if (e == nullptr) {
        return LINGLONG_ERR("failed to get elf");
    }
    auto clean_elf = utils::finally::finally([e] {
        elf_end(e);
    });

    GElf_Ehdr ehdr;
    if (gelf_getehdr(e, &ehdr) == nullptr) {
        return LINGLONG_ERR("failed to get elf header");
    }

    int file_data_encoding = ehdr.e_ident[EI_DATA];
    if (file_data_encoding != ELFDATA2LSB && file_data_encoding != ELFDATA2MSB) {
        return LINGLONG_ERR("unknown elf endianness");
    }
    bool is_lsb = (file_data_encoding == ELFDATA2LSB);

    int elf_class = gelf_getclass(e);

    if (elf_class == ELFCLASS64) {
        return addSectionImpl<Elf64_Ehdr, Elf64_Shdr>(fd, e, name, data, size, is_lsb);
    } else if (elf_class == ELFCLASS32) {
        return addSectionImpl<Elf32_Ehdr, Elf32_Shdr>(fd, e, name, data, size, is_lsb);
    } else {
        return LINGLONG_ERR("unknown elf class");
    }
}

} // namespace linglong::package
