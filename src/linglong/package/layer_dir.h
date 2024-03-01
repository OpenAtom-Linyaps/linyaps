/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_DIR_H_
#define LINGLONG_PACKAGE_LAYER_DIR_H_

#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/utils/error/error.h"

#include <QDir>

namespace linglong::package {

class LayerDir : public QDir
{
public:
    using QDir::QDir;
    utils::error::Result<api::types::v1::PackageInfo> info() const;
    utils::error::Result<QByteArray> rawInfo() const;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_DIR_H_ */
