/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProjectSource.hpp"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QUrl>

namespace linglong::builder {

class SourceFetcher
{
public:
    explicit SourceFetcher(api::types::v1::BuilderProjectSource source, const QDir &cacheDir);

    auto fetch(QDir destination) noexcept -> utils::error::Result<void>;

    void setCommand(std::shared_ptr<utils::Cmd> cmd) { this->m_cmd = cmd; }

private:
    QString getSourceName();
    QDir cacheDir;
    api::types::v1::BuilderProjectSource source;
    std::shared_ptr<utils::Cmd> m_cmd = std::make_shared<utils::Cmd>("sh");
};

} // namespace linglong::builder
