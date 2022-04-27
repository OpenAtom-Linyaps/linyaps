/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "builder.h"
#include "module/package/bundle.h"
#include "module/util/status_code.h"

namespace linglong {
namespace builder {

class LinglongBuilder
    : public QObject
    , public Builder
{
    Q_OBJECT
public:
    linglong::util::Error create(const QString &projectName) override;

    linglong::util::Error build() override;

    linglong::util::Error exportBundle(const QString &outputFilepath) override;

    linglong::util::Error push(const QString &bundleFilePath, bool force) override;

    linglong::util::Error run() override;

    linglong::util::Error initRepo();
};

} // namespace builder
} // namespace linglong