/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <list>

namespace linglong::repo {

using linglong::api::types::v1::PackageInfoV2;
using linglong::api::types::v1::Repo;

using PackagesWithRepo = std::pair<Repo, std::vector<PackageInfoV2>>;
using PackageWithRepo =
  std::pair<std::reference_wrapper<const Repo>, std::reference_wrapper<const PackageInfoV2>>;

class RemotePackages
{
public:
    RemotePackages() = default;
    RemotePackages(RemotePackages &&) = default;
    RemotePackages &operator=(RemotePackages &&) = default;

    RemotePackages &addPackages(Repo repo, std::vector<PackageInfoV2> packages);

    const std::list<PackagesWithRepo> &getRepoPackages() const { return repoPackages; }

    bool empty() const { return repoPackages.empty(); }

    const utils::error::Result<PackageWithRepo> getLatestPackage() const;
    std::vector<std::string> getReferenceModules(const package::Reference &ref) const;

private:
    std::list<PackagesWithRepo> repoPackages;
};

} // namespace linglong::repo
