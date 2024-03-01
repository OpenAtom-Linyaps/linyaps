/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_ARCHITECTURE_H_
#define LINGLONG_PACKAGE_ARCHITECTURE_H_

#include "linglong/utils/error/error.h"

#include "nlohmann/json.hpp"

#include <QString>

namespace linglong::package {

class Architecture
{
public:
    enum Value : quint32 {
        UNKNOW,
        X86_64,
        ARM64,
    };

    explicit Architecture(Value value = UNKNOW);
    explicit Architecture(const QString &raw);

    QString toString() const noexcept;
    QString getTriplet() const noexcept;

    bool operator==(const Architecture &that) const noexcept { return this->v == that.v; }
    bool operator!=(const Architecture &that) const noexcept { return this->v != that.v; }

    static utils::error::Result<Architecture> parse(const QString &raw) noexcept;

private:
    Value v;
};

} // namespace linglong::package

#endif
