/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProjectSource.hpp"
#include "linglong/utils/error/error.h"

#include <QFileInfo>
#include <QObject>
#include <QUrl>
#include <QDir>

namespace linglong::builder {

class SourceFetcher
{
public:
    explicit SourceFetcher(api::types::v1::BuilderProjectSource source,
                           api::types::v1::BuilderConfig cfg,
                           const QDir &cacheDir);

    auto fetch(QDir destination) noexcept -> utils::error::Result<void>;

private:
    QString getSourceName();
    QDir cacheDir;
    api::types::v1::BuilderProjectSource source;
    api::types::v1::BuilderConfig cfg;
};

} // namespace linglong::builder
