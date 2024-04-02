/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
#define LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProjectSource.hpp"
#include "linglong/utils/error/error.h"

#include <QFileInfo>
#include <QObject>
#include <QUrl>

namespace linglong::builder {

class SourceFetcher
{
public:
    explicit SourceFetcher(api::types::v1::BuilderProjectSource s,
                           api::types::v1::BuilderConfig cfg);

    auto fetch(QDir destination) noexcept -> utils::error::Result<void>;

private:
    QString getSourceName();
    api::types::v1::BuilderProjectSource source;
    api::types::v1::BuilderConfig cfg;
};

} // namespace linglong::builder

#endif // LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
