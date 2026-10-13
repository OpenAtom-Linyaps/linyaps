// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "packageinfo_handler.h"

#include <fmt/format.h>

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

error::Result<void> validatePackageId(const std::string &id)
{
    LINGLONG_TRACE("validate package id");

    if (id.empty()) {
        return LINGLONG_ERR("package id is empty");
    }

    // the id is used as a single path component when building host directories
    // such as the app runtime dir and the private dir, so a malformed id from a
    // layer or uab must not be able to escape its intended directory
    if (id.find('/') != std::string::npos || id.find('\\') != std::string::npos) {
        return LINGLONG_ERR(fmt::format("package id {} contains path separators", id));
    }
    if (id == "." || id == "..") {
        return LINGLONG_ERR(fmt::format("package id {} is not a usable directory name", id));
    }
    if (id.find('\0') != std::string::npos) {
        return LINGLONG_ERR("package id contains a null byte");
    }

    return LINGLONG_OK;
}

error::Result<api::types::v1::PackageInfoV2> parsePackageInfoFile(const std::filesystem::path &path)
{
    LINGLONG_TRACE("parse package info from file: " + path.string());

    auto pkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfoV2>(path);
    if (pkgInfo) {
        auto idValid = validatePackageId(pkgInfo->id);
        if (!idValid) {
            return LINGLONG_ERR(idValid);
        }
        return pkgInfo;
    }

    LogD("not PackageInfoV2, parse with PackageInfo: {}", pkgInfo.error());
    auto oldPkgInfo = serialize::LoadJSONFile<api::types::v1::PackageInfo>(path);
    if (!oldPkgInfo) {
        return LINGLONG_ERR(oldPkgInfo.error());
    }

    auto convertedInfo = toPackageInfoV2(*oldPkgInfo);
    auto idValid = validatePackageId(convertedInfo.id);
    if (!idValid) {
        return LINGLONG_ERR(idValid);
    }
    return convertedInfo;
}

} // namespace linglong::utils::serialize
