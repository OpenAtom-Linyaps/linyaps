/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/utils/serialize/json.h"

namespace linglong::package {

utils::error::Result<api::types::v1::PackageInfo> LayerDir::info() const
{
    LINGLONG_TRACE("get layer info from " + this->absolutePath());

    auto info =
      utils::serialize::LoadJSONFile<api::types::v1::PackageInfo>(this->filePath("info.json"));
    if (info) {
        return info;
    }

    // FIXME: code below should be removed later.

    qWarning() << info.error();
    qWarning() << "Try fallback to v0 API";
    auto info0 =
      utils::serialize::LoadJSONFile<api::types::v0::PackageInfo>(this->filePath("info.json"));
    if (!info0) {
        return LINGLONG_ERR("after fallback to v0", info0);
    }

    nlohmann::json json = *info0;
    json["appId"] = json["appid"];
    json.erase("appid");

    if (json["arch"].size() != 1) {
        return LINGLONG_ERR("unexpected v0 info.json");
    }
    json["arch"] = json["arch"][0];

    json["channel"] = "main";

    info = utils::serialize::LoadJSON<api::types::v1::PackageInfo>(json);
    if (!info) {
        return LINGLONG_ERR(info);
    }

    return info;
}

} // namespace linglong::package
