/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QString>

namespace linglong {
namespace util {

QDir userRuntimeDir();

QStringList parseExec(const QString &exec);

} // namespace util
} // namespace linglong
