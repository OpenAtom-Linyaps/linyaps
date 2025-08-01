/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_file.h"
#include "linglong/utils/error/error.h"

#include <QString>
#include <QUuid>
#include <string>


namespace linglong::package {

class MockLayerPackager;

class LayerPackager : public QObject
{
    friend class MockLayerPackager;
public:
    explicit LayerPackager();
    LayerPackager(const LayerPackager &) = delete;
    LayerPackager(LayerPackager &&) = delete;
    LayerPackager &operator=(const LayerPackager &) = delete;
    LayerPackager &operator=(LayerPackager &&) = delete;
    ~LayerPackager() override;
    utils::error::Result<QSharedPointer<LayerFile>> pack(const LayerDir &dir,
                                                         const QString &layerFilePath) const;
    utils::error::Result<LayerDir> unpack(LayerFile &file);
    utils::error::Result<void> setCompressor(const QString &compressor) noexcept;
    const QDir &getWorkDir() const;
private:
    QDir workDir;
    QString compressor = "lzma";
    bool isMounted = false;
    // 初始化工作目录
    utils::error::Result<void> initWorkDir();
    // 检查erofs-fuse命令是否存在
    virtual utils::error::Result<bool> checkErofsFuseExists() const;
    // 创建目录，用于单元测试
    virtual utils::error::Result<void> mkdirDir(const std::string &path) noexcept;
    // 判断fd是否可在其他进程读取
    virtual bool isFileReadable(const std::string &path) const;
    // LayerFile的save并不能用于保存无权限的fd，所以需要单独实现
    virtual utils::error::Result<void> copyFile(LayerFile &file, const std::string &path, const int64_t offset) const;
};

} // namespace linglong::package
