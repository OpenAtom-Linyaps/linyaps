/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/packageinfo_handler.h"

#include <fstream>

namespace linglong::package {

utils::error::Result<api::types::v1::PackageInfoV2> LayerDir::info() const
{
    LINGLONG_TRACE("get layer info from " + this->absolutePath());

    auto info = utils::parsePackageInfo(this->filePath("info.json"));
    if (!info) {
        return LINGLONG_ERR(info);
    }

    return info;
}

QString LayerDir::filesDirPath() const noexcept
{
    return this->absoluteFilePath("files");
}

bool LayerDir::valid() const noexcept
{
    return this->exists("info.json");
}

} // namespace linglong::package
