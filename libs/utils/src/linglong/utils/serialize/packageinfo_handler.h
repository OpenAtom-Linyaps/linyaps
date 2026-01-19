// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/json.h"

#include <filesystem>

#define PACKAGE_INFO_VERSION "1.0"

namespace linglong::utils::serialize {

api::types::v1::PackageInfoV2 toPackageInfoV2(const api::types::v1::PackageInfo &oldInfo);
error::Result<api::types::v1::PackageInfoV2>
parsePackageInfoFile(const std::filesystem::path &path);

template <typename T>
error::Result<api::types::v1::PackageInfoV2> parsePackageInfo(const T &contents)
{
    LINGLONG_TRACE("parse package info from str");

    auto pkgInfo = serialize::LoadJSON<api::types::v1::PackageInfoV2>(contents);
    if (pkgInfo) {
        return pkgInfo;
    }

    LogD("not PackageInfoV2, parse with PackageInfo: {}", pkgInfo.error());
    auto oldPkgInfo = serialize::LoadJSON<api::types::v1::PackageInfo>(contents);
    if (!oldPkgInfo) {
        return LINGLONG_ERR(oldPkgInfo.error());
    }

    return toPackageInfoV2(*oldPkgInfo);
}

} // namespace linglong::utils::serialize
