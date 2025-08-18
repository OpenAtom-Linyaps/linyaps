/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/package/architecture.h"
#include "linglong/package/version.h"

#include <QString>

namespace linglong::package {

// This class is a reference to a tier, use as a tier ID.
class Reference final
{
public:
    static utils::error::Result<Reference> parse(const std::string &raw) noexcept;
    static utils::error::Result<Reference> parse(const QString &raw) noexcept;
    static utils::error::Result<Reference> create(const QString &channel,
                                                  const QString &id,
                                                  const Version &version,
                                                  const Architecture &architecture) noexcept;
    static utils::error::Result<Reference>
    fromPackageInfo(const api::types::v1::PackageInfoV2 &info) noexcept;
    static QVariantMap toVariantMap(const Reference &ref) noexcept;
    static utils::error::Result<Reference> fromVariantMap(const QVariantMap &data) noexcept;
    static utils::error::Result<Reference>
    fromBuilderProject(const api::types::v1::BuilderProject &project) noexcept;

    QString channel;
    QString id;
    Version version;
    Architecture arch;

    [[nodiscard]] QString toString() const noexcept;
    friend bool operator!=(const Reference &lhs, const Reference &rhs) noexcept;
    friend bool operator==(const Reference &lhs, const Reference &rhs) noexcept;

private:
    Reference(const QString &channel,
              const QString &id,
              const Version &version,
              const Architecture &architecture);
};

struct ReferenceWithRepo
{
    api::types::v1::Repo repo;
    Reference reference;
};

} // namespace linglong::package

// Note: declare here, so we can use std::unordered_map<Reference, ...> in other place
template <>
struct std::hash<linglong::package::Reference>
{
    size_t operator()(const linglong::package::Reference &ref) const noexcept
    {
        size_t hash = 0;
        hash ^= qHash(ref.channel);
        hash ^= qHash(ref.id) << 1;
        hash ^= qHash(ref.version.toString()) << 2;
        hash ^= qHash(ref.arch.toString()) << 3;
        return hash;
    }
};
