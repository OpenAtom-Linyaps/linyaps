/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/module.h"

namespace linglong::package {

Module::Module(Value value)
    : v(value)
{
}

QString Module::toString() const noexcept
{
    switch (this->v) {
    case UNKNOW:
        return "unknown";
    case RUNTIME:
        return "runtime";
    case DEVELOP:
        return "develop";
    }
}

utils::error::Result<Module> Module::parse(const QString &raw) noexcept
try {
    return Module(raw);
} catch (const std::exception &e) {
    return Err(-1, e.what());
}

Module::Module(const QString &raw)
    : v([&raw]() {
        if (raw == "runtime") {
            return RUNTIME;
        }

        if (raw == "develop") {
            return DEVELOP;
        }

        throw std::runtime_error("unknown module");
    }())
{
}

} // namespace linglong::package
