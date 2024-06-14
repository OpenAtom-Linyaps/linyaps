// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"

#define PACKAGE_INFO_VERSION "1.0"
namespace linglong::utils {

error::Result<api::types::v1::PackageInfoV2> toPackageInfoV2(const api::types::v1::PackageInfo &oldInfo)
{
    LINGLONG_TRACE("convert PackageInfo to PackageInfoV2");

    auto info = api::types::v1::PackageInfoV2{
        .arch = oldInfo.arch,
        .base = oldInfo.base,
        .channel = oldInfo.channel,
        .command = oldInfo.command,
        .description = oldInfo.description,
        .id = oldInfo.appid,
        .kind = oldInfo.kind,
        .packageInfoV2Module = oldInfo.packageInfoModule,
        .name = oldInfo.name,
        .permissions = oldInfo.permissions,
        .runtime = oldInfo.runtime,
        .schemaVersion = PACKAGE_INFO_VERSION,
        .size = oldInfo.size,
        .version = oldInfo.version,
    };

    return info;
}

error::Result<api::types::v1::PackageInfoV2> parsePackageInfo(const QString &path)
{
    LINGLONG_TRACE("parse package info from file: " + path);

    auto pkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfoV2>(path);
    if (pkgInfo) {
        return pkgInfo;
    }

    qDebug() << "not PackageInfoV2, parse with PackageInfo";
    auto oldPkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfo>(path);
    if (!oldPkgInfo) {
          return LINGLONG_ERR(toPackageInfoV2(*oldPkgInfo));
    }

    return toPackageInfoV2(*oldPkgInfo);
}

error::Result<api::types::v1::PackageInfoV2> parsePackageInfo(nlohmann::json &json)
{
    LINGLONG_TRACE("parse package info from json");

    auto pkgInfo = serialize::LoadJSON<api::types::v1::PackageInfoV2>(json);
    if (pkgInfo) {
        return pkgInfo;
    }

    qDebug() << "not PackageInfoV2, parse with PackageInfo";
    auto oldPkgInfo = serialize::LoadJSON<api::types::v1::PackageInfo>(json);
    if (!oldPkgInfo) {
          return LINGLONG_ERR(toPackageInfoV2(*oldPkgInfo));
    }

    return toPackageInfoV2(*oldPkgInfo);
}

}; // namespace linglong::utils
