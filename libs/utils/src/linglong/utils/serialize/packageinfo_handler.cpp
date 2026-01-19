// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "packageinfo_handler.h"

namespace linglong::utils::serialize {

api::types::v1::PackageInfoV2 toPackageInfoV2(const api::types::v1::PackageInfo &oldInfo)
{
    return api::types::v1::PackageInfoV2{
        .arch = oldInfo.arch,
        .base = oldInfo.base,
        .channel = oldInfo.channel.value_or("main"),
        .command = oldInfo.command,
        .compatibleVersion = std::nullopt,
        .description = oldInfo.description,
        .id = oldInfo.appid,
        .kind = oldInfo.kind,
        .packageInfoV2Module = oldInfo.packageInfoModule,
        .name = oldInfo.name,
        .permissions = oldInfo.permissions,
        .runtime = oldInfo.runtime,
        .schemaVersion = PACKAGE_INFO_VERSION,
        .size = oldInfo.size,
        .uuid = std::nullopt,
        .version = oldInfo.version,
    };
}

error::Result<api::types::v1::PackageInfoV2> parsePackageInfoFile(const std::filesystem::path &path)
{
    LINGLONG_TRACE("parse package info from file: " + path.string());

    auto pkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfoV2>(path);
    if (pkgInfo) {
        return pkgInfo;
    }

    LogD("not PackageInfoV2, parse with PackageInfo: {}", pkgInfo.error());
    auto oldPkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfo>(path);
    if (!oldPkgInfo) {
        return LINGLONG_ERR(oldPkgInfo.error());
    }

    return toPackageInfoV2(*oldPkgInfo);
}

} // namespace linglong::utils::serialize
