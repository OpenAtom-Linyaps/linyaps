/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_MODULE_H_
#define LINGLONG_PACKAGE_MODULE_H_

#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::package {
class Module
{
public:
    enum Value : quint8 {
        UNKNOW,
        RUNTIME,
        DEVELOP,
    };

    explicit Module(Value value = UNKNOW);
    explicit Module(const QString &raw);

    QString toString() const noexcept;

    static utils::error::Result<Module> parse(const QString &raw) noexcept;

private:
    Value v;
};

} // namespace linglong::package

#endif
