/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/architecture.h"

#include <elf.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace linglong::package {
Architecture::Architecture(Value value)
    : v(value)
{
}

QString Architecture::toString() const noexcept
{
    switch (this->v) {
    case X86_64:
        return "x86_64";
    case ARM64:
        return "arm64";
    case LOONGARCH64:
        return "loongarch64";
    case LOONG64:
        return "loong64";
    case SW64:
        return "sw64";
    case UNKNOW:
        [[fallthrough]];
    default:
        return "unknow";
    }
}

QString Architecture::getTriplet() const noexcept
{
    switch (this->v) {
    case UNKNOW:
        return "unknow";
    case X86_64:
        return "x86_64-linux-gnu";
    case ARM64:
        return "aarch64-linux-gnu";
    case LOONG64:
        return "loongarch64-linux-gnu";
    case SW64:
        return "sw_64-linux-gnu";
    case LOONGARCH64:
        return "loongarch64-linux-gnu";
    }
    return "unknow";
}

utils::error::Result<Architecture> Architecture::parse(const std::string &raw) noexcept
try {
    return Architecture(raw);

} catch (const std::exception &e) {
    LINGLONG_TRACE("parse architecture");
    return LINGLONG_ERR(e);
}

Architecture::Architecture(const std::string &raw)
    : v([&raw]() {
        if (raw == "x86_64") {
            return X86_64;
        }

        if (raw == "arm64") {
            return ARM64;
        }

        if (raw == "loongarch64") {
            return LOONGARCH64;
        }

        if (raw == "loong64") {
            return LOONG64;
        }

        if (raw == "sw64") {
            return SW64;
        }

        throw std::runtime_error("unknow architecture");
    }())
{
}

std::string Architecture::getInterpreter()
{
    static std::string interpreterPath = "";
    if (!interpreterPath.empty()) {
        return interpreterPath;
    }
    // 打开可执行文件
    std::ifstream file("/proc/self/exe", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open executable file" << std::endl;
        return "";
    }
    // 读取 ELF 头
    Elf64_Ehdr ehdr;
    file.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));
    if (!file || std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        std::cerr << "Not a valid ELF file." << std::endl;
        return "";
    }
    // 移动到程序头表偏移处
    file.seekg(ehdr.e_phoff, std::ios::beg);
    // 读取程序头表
    std::vector<Elf64_Phdr> phdrs(ehdr.e_phnum);
    file.read(reinterpret_cast<char *>(phdrs.data()), ehdr.e_phnum * sizeof(Elf64_Phdr));
    if (!file) {
        std::cerr << "Failed to read program headers." << std::endl;
        return "";
    }
    // 查找 PT_INTERP 段
    for (const auto &phdr : phdrs) {
        if (phdr.p_type == PT_INTERP) {
            // 获取解释器的偏移和大小
            std::vector<char> interpreter(phdr.p_filesz);
            file.seekg(phdr.p_offset, std::ios::beg);
            file.read(interpreter.data(), phdr.p_filesz);
            if (!file) {
                std::cerr << "Failed to read interpreter." << std::endl;
                return "";
            }
            interpreterPath = interpreter.data();
            return interpreterPath;
        }
    }
    return "";
}

utils::error::Result<Architecture> Architecture::currentCPUArchitecture() noexcept
{
    auto interpreter = getInterpreter();
    if (interpreter == "/lib64/ld-linux-x86-64.so.2") {
        return Architecture::parse("x86_64");
    }
    if (interpreter == "/lib/ld-linux-aarch64.so.1") {
        return Architecture::parse("arm64");
    }
    if (interpreter == "/lib64/ld-linux-loongarch-lp64d.so.1") {
        return Architecture::parse("loong64");
    }
    if (interpreter == "/lib64/ld.so.1") {
        return Architecture::parse("loongarch64");
    }
    if (interpreter == "/lib/ld-linux-sw-64.so.2") {
        return Architecture::parse("sw64");
    }
    return Architecture::parse("");
};

} // namespace linglong::package
