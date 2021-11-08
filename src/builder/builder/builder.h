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

#include <QObject>

#include "module/util/result.h"

namespace linglong {
namespace builder {

class Builder
{
public:
    virtual util::Result create(const QString &projectName) = 0;

    virtual util::Result build() = 0;

    virtual util::Result exportBundle(const QString &outputFilepath) = 0;

    virtual util::Result push(const QString &repoURL, bool force) = 0;
};

} // namespace builder
} // namespace linglong