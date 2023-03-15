/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/package/ref_new.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

namespace linglong {
namespace package {
namespace refact {
namespace defaults {
const QString arch = "x86_64"; // FIXME: should be generated
const QString module = "runtime";
} // namespace defaults

const char *const Ref::verifyRegexp = "^[\\w\\d][-._\\w\\d]*$";

Ref::Ref(const QString &str)
{
    // ${packageID}/${version}[/${arch}[/${module}]]

    QStringList slice = str.split("/");

    if (slice.length() >= 5)
        return;

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

    if (arch.isEmpty()) {
        arch = defaults::arch;
    }
    if (module.isEmpty()) {
        module = defaults::module;
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
    QRegularExpression regexExp(verifyRegexp);
    QStringRef list[] = { &this->packageID, &this->version, &this->arch, &this->module };

    for (const auto &str : list) {
        if (!regexExp.match(str).hasMatch()) {
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
