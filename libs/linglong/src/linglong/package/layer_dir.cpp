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

utils::error::Result<api::types::v1::MinifiedInfo> LayerDir::minifiedInfo() const
{
    LINGLONG_TRACE("get minified info from " + absolutePath())
    auto filePath = absoluteFilePath("minified.json");

    std::fstream stream{ filePath.toStdString() };
    if (!stream.is_open()) {
        return LINGLONG_ERR(QString{ "couldn't open file %1" }.arg(filePath));
    }

    nlohmann::json content;
    try {
        content = nlohmann::json::parse(stream);
    } catch (nlohmann::json::parse_error &e) {
        return LINGLONG_ERR(QString{ "parsing minified.json error: %1" }.arg(e.what()));
    }

    return content.get<api::types::v1::MinifiedInfo>();
}

bool LayerDir::hasMinified() const noexcept
{
    return this->exists("minified.json");
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
