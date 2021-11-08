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

namespace linglong {
namespace builder {

class BstBuilder
    : public QObject
    , public Builder
{
    Q_OBJECT
public:
    util::Result create(const QString &projectName) override;

    util::Result Build() override;

    util::Result Export(const QString &outputFilepath) override;

    util::Result Push(const QString &repoURL, bool force) override;
};

} // namespace builder
} // namespace linglong