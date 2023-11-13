/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_REFERENCE_H_
#define LINGLONG_PACKAGE_REFERENCE_H_

#include "linglong/package/architecture.h"
#include "linglong/package/module.h"
#include "linglong/package/version.h"

#include <QString>

namespace linglong::package {

// This class is a reference to a tier, use as a tier ID.
struct Reference final
{
    static utils::error::Result<Reference> parse(const QString &raw) noexcept;
    explicit Reference(const QString &raw);

    QString id;
    Version version;
    Architecture arch;
    Module module;
    QString channel;

    QString toString() const noexcept;

private:
    explicit Reference(const QStringList &components);
};

} // namespace linglong::package

#endif
