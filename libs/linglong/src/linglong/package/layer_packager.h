/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_PACKAGER_H_
#define LINGLONG_PACKAGE_LAYER_PACKAGER_H_

#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_file.h"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::package {

class LayerPackager : public QObject
{
public:
    explicit LayerPackager(const QDir &workDir = QDir("/tmp/linglong-layer"));
    LayerPackager(const LayerPackager &) = delete;
    LayerPackager(LayerPackager &&) = delete;
    LayerPackager &operator=(const LayerPackager &) = delete;
    LayerPackager &operator=(LayerPackager &&) = delete;
    ~LayerPackager() override;
    utils::error::Result<QSharedPointer<LayerFile>> pack(const LayerDir &dir,
                                                         const QString &layerFilePath) const;
    utils::error::Result<LayerDir> unpack(LayerFile &file);

private:
    QDir workDir;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_PACKAGER_H_ */
