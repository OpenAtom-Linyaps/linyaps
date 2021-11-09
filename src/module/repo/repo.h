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

#include <QString>

namespace linglong {

namespace package {
class Ref;
}

namespace repo {

// FIXME: move to class LocalRepo
package::Ref latestOf(const QString &appID);

QString rootOfLayer(const package::Ref &ref);

} // namespace repo
} // namespace linglong