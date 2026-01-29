/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/architecture.h"

#include "linglong/utils/log/log.h"

#include <elf.h>

#include <QSysInfo>

#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>

namespace linglong::package {
Architecture::Architecture(Value value)
    : v(value)
{
}

std::string Architecture::toString() const noexcept
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
    case MIPS64:
        return "mips64";
    case UNKNOW:
        [[fallthrough]];
    default:
        return "unknown";
    }
}

std::string Architecture::getTriplet() const noexcept
{
    switch (this->v) {
    case UNKNOW:
        return "unknown";
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
    case MIPS64:
        return "mips64el-linux-gnuabi64";
    }
    return "unknown";
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

        if (raw == "mips64") {
            return MIPS64;
        }

        throw std::runtime_error("unknow architecture");
    }())
{
}

namespace {
bool isNewWorldLoongArch()
{
    static std::optional<bool> isLoongArch;
    if (isLoongArch.has_value()) {
        return isLoongArch.value();
    }

    // 打开可执行文件
    std::ifstream file("/proc/self/exe", std::ios::binary);
    if (!file) {
        LogE("Failed to open executable file");
        isLoongArch = false;
        return false;
    }

    // 读取 ELF 头
    Elf64_Ehdr ehdr;
    file.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));
    if (!file || std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        LogE("Not a valid ELF file.");
        isLoongArch = false;
        return false;
    }

    auto val = ehdr.e_flags >> 6U;
    isLoongArch = ((val & 1U) == 1);
    return isLoongArch.value();
}
} // namespace

const Architecture &Architecture::currentCPUArchitecture()
{
    auto currentArch = []() {
        auto arch = QSysInfo::currentCpuArchitecture().toStdString();

        if (arch == "sw_64") {
            arch = "sw64";
        }

        if (arch == "loongarch64" || arch == "loong64") {
            if (isNewWorldLoongArch()) {
                arch = "loong64";
            } else {
                arch = "loongarch64";
            }
        }
        return arch;
    };

    // throw exception if architecture is unknown
    static Architecture arch(currentArch());
    return arch;
}

} // namespace linglong::package
