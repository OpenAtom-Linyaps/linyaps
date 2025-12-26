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

namespace linglong::package {

// This class is a reference to a tier, use as a tier ID.
class Reference final
{
public:
    static utils::error::Result<Reference> parse(const std::string &raw) noexcept;
    static utils::error::Result<Reference> create(const std::string &channel,
                                                  const std::string &id,
                                                  const Version &version,
                                                  const Architecture &architecture) noexcept;
    static utils::error::Result<Reference>
    fromPackageInfo(const api::types::v1::PackageInfoV2 &info) noexcept;
    static utils::error::Result<Reference>
    fromBuilderProject(const api::types::v1::BuilderProject &project) noexcept;

    std::string channel;
    std::string id;
    Version version;
    Architecture arch;

    [[nodiscard]] std::string toString() const noexcept;
    friend bool operator!=(const Reference &lhs, const Reference &rhs) noexcept;
    friend bool operator==(const Reference &lhs, const Reference &rhs) noexcept;

private:
    Reference(const std::string &channel,
              const std::string &id,
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
        auto hash_combine = [](std::size_t &seed, std::size_t value) {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
        };

        std::size_t seed = 0;
        auto hasher = std::hash<std::string>{};
        hash_combine(seed, hasher(ref.channel));
        hash_combine(seed, hasher(ref.id));
        hash_combine(seed, hasher(ref.version.toString()));
        hash_combine(seed, hasher(ref.arch.toString()));
        return seed;
    }
};
