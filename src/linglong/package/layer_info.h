/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_INFO_H_
#define LINGLONG_PACKAGE_LAYER_INFO_H_

#include "linglong/package/layer/LayerInfo.hpp"
#include "linglong/utils/error/error.h"

namespace linglong::package {

const char *const layerInfoVerison = "0.1";

utils::error::Result<layer::LayerInfo> fromJson(const QByteArray &rawData);
utils::error::Result<QByteArray> toJson(layer::LayerInfo &layerInfo);

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_INFO_H_ */
