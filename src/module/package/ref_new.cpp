/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/package/ref_new.h"

#include <QDebug>
#include <QStringList>

namespace linglong {
namespace package {
namespace refact {
namespace defaults {
const QString arch = "x86_64"; // FIXME: should be generated
const QString repo = "repo";
const QString channel = "stable";
const QString module = "runtime";
} // namespace defaults

Ref::Ref(const QString &str)
{
    QStringList slice = str.split("/");
    switch (slice.length()) {
    case 4:
        module = slice.value(3);
        [[fallthrough]];
    case 3:
        arch = slice.value(2);
        [[fallthrough]];
    case 2:
        packageID = slice.value(0);
        version = slice.value(1);
    }
}

Ref::Ref(const QString &packageID,
         const QString &version,
         const QString &arch,
         const QString &module)
    : packageID(packageID)
    , version(version)
    , arch(arch)
    , module(module)
{
}

bool Ref::isVaild() const
{
    // Fixme: this regex maybe too simple.
    QRegularExpression regexExp(LINGLONG_FRAGMENT_REGEXP);
    QStringList tmpList;

    tmpList << this->packageID << this->version << this->arch << this->module;

    for (const auto tmpStr : tmpList) {
        if (!regexExp.match(tmpStr).hasMatch()) {
            return false;
        }
    }

    return true;
}

QString Ref::toString() const
{
    return QString("%1/%2/%3/%4").arg(packageID, version, arch, module);
}
} // namespace refact
} // namespace package
} // namespace linglong
