/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
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

namespace linglong {
namespace builder {

class BstBuilder
    : public QObject
    , public Builder
{
    Q_OBJECT
public:
    linglong::util::Error create(const QString &projectName) override;

    linglong::util::Error build() override;

    linglong::util::Error exportBundle(const QString &outputFilepath) override;

    // util::Result push(const QString &repoURL, bool force) override;
    linglong::util::Error push(const QString &bundleFilePath, bool force) override;

    linglong::util::Error run() override { return NoError(); }
};

} // namespace builder
} // namespace linglong