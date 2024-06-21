/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_DIR_H_
#define LINGLONG_PACKAGE_LAYER_DIR_H_

#include "linglong/api/types/v1/MinifiedInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/utils/error/error.h"

#include <QDir>

namespace linglong::package {

class LayerDir : public QDir
{
public:
    using QDir::QDir;

    [[nodiscard]] utils::error::Result<api::types::v1::PackageInfoV2> info() const;
    [[nodiscard]] utils::error::Result<std::optional<api::types::v1::MinifiedInfo>>
    minifiedInfo() const;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_DIR_H_ */
