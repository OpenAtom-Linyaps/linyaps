/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_DIR_H_
#define LINGLONG_PACKAGE_LAYER_DIR_H_

#include "linglong/package/info.h"
#include "linglong/utils/error/error.h"

#include <QDir>

namespace linglong::package {

class LayerDir : public QDir
{
public:
    using QDir::QDir;
    ~LayerDir();
    utils::error::Result<QSharedPointer<Info>> info() const;
    utils::error::Result<QByteArray> rawInfo() const;
    void setCleanStatus(bool status);

private:
    bool cleanup = true;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_DIR_H_ */
