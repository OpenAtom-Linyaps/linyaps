/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "remote_packages.h"

namespace linglong::repo {

RemotePackages &RemotePackages::addPackages(Repo repo, std::vector<PackageInfoV2> packages)
{
    repoPackages.emplace_back(std::make_pair(std::move(repo), std::move(packages)));
    return *this;
}

const utils::error::Result<PackageWithRepo> RemotePackages::getLatestPackage() const
{
    LINGLONG_TRACE("get latest package");

    if (empty()) {
        return LINGLONG_ERR("packages is empty");
    }

    auto compare = [](const auto &a, const auto &b) -> bool {
        auto versionA = package::Version::parse(a.version);
        auto versionB = package::Version::parse(b.version);

        if (!versionB) {
            return false;
        } else if (!versionA) {
            return true;
        }

        return *versionA < *versionB;
    };

    std::optional<PackageWithRepo> latest;
    for (const auto &packages : repoPackages) {
        auto max = std::max_element(packages.second.begin(), packages.second.end(), compare);
        if (max != packages.second.end() && (!latest || compare(latest->second.get(), *max))) {
            latest = std::make_pair(std::ref(packages.first), std::ref(*max));
        }
    }

    return *latest;
}

std::vector<std::string> RemotePackages::getReferenceModules(const package::Reference &ref) const
{
    std::vector<std::string> modules;
    for (const auto &packages : repoPackages) {
        for (const auto &package : packages.second) {
            if (package.id == ref.id && package.channel == ref.channel
                && package.version == ref.version.toString()
                && package.arch[0] == ref.arch.toString()) {
                modules.emplace_back(package.packageInfoV2Module);
            }
        }
    }
    return modules;
}

} // namespace linglong::repo
