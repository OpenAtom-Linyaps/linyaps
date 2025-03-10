/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <QString>

#include <filesystem>
#include <string>

namespace linglong::package {

class Architecture
{
public:
    enum Value : quint32 {
        UNKNOW,
        X86_64,
        ARM64,
        LOONGARCH64,
        LOONG64,
        SW64,
        MIPS64,
    };

    explicit Architecture(Value value = UNKNOW);
    explicit Architecture(const std::string &raw);

    QString toString() const noexcept;
    QString getTriplet() const noexcept;

    bool operator==(const Architecture &that) const noexcept { return this->v == that.v; }

    bool operator!=(const Architecture &that) const noexcept { return this->v != that.v; }

    static utils::error::Result<Architecture> parse(const std::string &raw) noexcept;

    static utils::error::Result<Architecture> currentCPUArchitecture() noexcept;

private:
    Value v;
};

} // namespace linglong::package
