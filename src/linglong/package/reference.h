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
class Reference final
{
public:
    static utils::error::Result<Reference> parse(const QString &raw) noexcept;
    static utils::error::Result<Reference> create(const QString &channel,
                                                  const QString &id,
                                                  const Version &version,
                                                  const Architecture &architecture) noexcept;

    QString channel;
    QString id;
    Version version;
    Architecture arch;

    QString toString() const noexcept;

private:
    Reference(const QString &channel,
              const QString &id,
              const Version &version,
              const Architecture &architecture);
};

} // namespace linglong::package

#endif
