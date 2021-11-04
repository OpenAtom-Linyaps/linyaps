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
    virtual util::Result Create(const QString &projectName) = 0;

    virtual util::Result Build() = 0;

    virtual util::Result Export(const QString &outputFilepath) = 0;

    virtual util::Result Push(const QString &repoURL, bool force) = 0;
};

} // namespace builder
} // namespace linglong