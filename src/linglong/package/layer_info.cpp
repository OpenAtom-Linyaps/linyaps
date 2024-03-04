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

try {
    layer::LayerInfo layerInfo;
    layerInfo = json::parse(rawData).get<layer::LayerInfo>();
    return layerInfo;
} catch (std::exception &e) {
    LINGLONG_TRACE("parse layer info from json");
    return LINGLONG_ERR("json::parse", e);
}

utils::error::Result<QByteArray> toJson(layer::LayerInfo &layerInfo)
try {
    QByteArray rawData;
    json jsonValue = layerInfo;
    rawData = QByteArray::fromStdString(jsonValue.dump());
    return rawData;
} catch (std::exception &e) {
    LINGLONG_TRACE("dump layer info to json");
    return LINGLONG_ERR("dump", e);
}

} // namespace linglong::package
