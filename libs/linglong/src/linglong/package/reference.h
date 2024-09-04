/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/package/architecture.h"
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
    static utils::error::Result<Reference>
    fromPackageInfo(const api::types::v1::PackageInfoV2 &info) noexcept;
    static QVariantMap toVariantMap(const Reference &ref) noexcept;
    static utils::error::Result<Reference> fromVariantMap(const QVariantMap &data) noexcept;

    QString channel;
    QString id;
    Version version;
    Architecture arch;

    [[nodiscard]] QString toString() const noexcept;

private:
    Reference(const QString &channel,
              const QString &id,
              const Version &version,
              const Architecture &architecture);
};

} // namespace linglong::package
