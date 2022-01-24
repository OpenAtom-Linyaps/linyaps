/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "uuid.h"

#include <QUuid>

namespace linglong {
namespace util {
QString genUuid()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}
} // namespace util
} // namespace linglong
