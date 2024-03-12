/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/architecture.h"

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
    }
}

utils::error::Result<Architecture> Architecture::parse(const QString &raw) noexcept
try {
    return Architecture(raw);

} catch (const std::exception &e) {
    LINGLONG_TRACE("parse architecture");
    return LINGLONG_ERR(e);
}

Architecture::Architecture(const QString &raw)
    : v([&raw]() {
        if (raw == "x86_64") {
            return X86_64;
        }

        if (raw == "arm64") {
            return ARM64;
        }

        throw std::runtime_error("unknow architecture");
    }())
{
}

} // namespace linglong::package
