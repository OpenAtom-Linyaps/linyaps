/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/util/qserializer/json.h"

namespace linglong::package {

LayerDir::~LayerDir()
{
    if (cleanup) {
        this->removeRecursively();
    }
}

void LayerDir::setCleanStatus(bool status)
{
    this->cleanup = status;
}

utils::error::Result<QSharedPointer<Info>> LayerDir::info() const
{
    LINGLONG_TRACE("get layer info form dir");

    const auto infoPath = QStringList{ this->absolutePath(), "info.json" }.join(QDir::separator());
    auto [info, err] = util::fromJSON<QSharedPointer<Info>>(infoPath);
    if (err) {
        return LINGLONG_ERR(err.message(), err.code());
    }

    return info;
}

utils::error::Result<QByteArray> LayerDir::rawInfo() const
{
    LINGLONG_TRACE("get raw layer info from dir");

    const auto infoPath = QStringList{ this->absolutePath(), "info.json" }.join(QDir::separator());
    QFile file(infoPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR("open info.json from layer dir", file);
    }

    QByteArray rawData = file.readAll();

    return rawData;
}

} // namespace linglong::package
