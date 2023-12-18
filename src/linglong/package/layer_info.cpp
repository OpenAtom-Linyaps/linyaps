/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_info.h"

#include "linglong/package/layer/Generators.hpp"
#include "linglong/package/layer_file.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace linglong::package {

using nlohmann::json;

utils::error::Result<layer::LayerInfo> fromJson(const QByteArray &rawData)
{
    layer::LayerInfo layerInfo;
    try {
        layerInfo = json::parse(rawData).get<layer::LayerInfo>();
    } catch (std::exception &e) {
        return LINGLONG_ERR(-1, "failed to parse json value, invalid data");
    }

    return layerInfo;
}

utils::error::Result<QByteArray> toJson(layer::LayerInfo &layerInfo)
{
    QByteArray rawData;
    json jsonValue = layerInfo;
    try {
        rawData = QByteArray::fromStdString(jsonValue.dump());
    } catch (std::exception &e) {
        return LINGLONG_ERR(-1, "failed to dump json value, invalid data");
    }

    return rawData;
}

} // namespace linglong::package