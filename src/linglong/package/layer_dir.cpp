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
    if(cleanup) {
        this->removeRecursively();
    }
}

void LayerDir::setCleanStatus(bool status)
{
    this->cleanup = status;
}

utils::error::Result<QSharedPointer<Info>> LayerDir::info() const
{
    const auto infoPath = QStringList { this->absolutePath(), "info.json"}.join(QDir::separator());
    auto [info, err] = util::fromJSON<QSharedPointer<Info>>(infoPath);
    if (err) {
        return LINGLONG_ERR(err.code(), "failed to parse info.json");
    }

    return info;
}

utils::error::Result<QByteArray> LayerDir::rawInfo() const
{
    const auto infoPath = QStringList { this->absolutePath(), "info.json"}.join(QDir::separator());
    QFile file(infoPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(-1, "failed to open info.json from layer dir");
    }
    
    QByteArray rawData = file.readAll();

    file.close();
    return rawData;
}

}