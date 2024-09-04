/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

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
    [[nodiscard]] bool hasMinified() const noexcept;
    [[nodiscard]] utils::error::Result<api::types::v1::MinifiedInfo> minifiedInfo() const;
    [[nodiscard]] QString filesDirPath() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
};

} // namespace linglong::package
