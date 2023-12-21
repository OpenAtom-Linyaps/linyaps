/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_PACKAGE_H_
#define LINGLONG_PACKAGE_LAYER_PACKAGE_H_

#include "linglong/package/layer_file.h"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::package {

class LayerPackager : public QObject
{
public:
    explicit LayerPackager(const QString &workDir = "/tmp/linglong-layer");
    ~LayerPackager();
    utils::error::Result<QSharedPointer<LayerFile>> pack(const LayerDir &dir, const QString &layerFilePath) const;
    utils::error::Result<QSharedPointer<LayerDir>> unpack(LayerFile &file, const QString &destnation);

private:
    QString workDir;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_PACKAGE_H_ */
