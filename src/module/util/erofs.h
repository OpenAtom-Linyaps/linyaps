/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_EROFS_H_
#define LINGLONG_SRC_MODULE_UTIL_EROFS_H_

#include "error.h"

namespace linglong {
namespace erofs {

// try mount with erofs-util
util::Error mount(const QString &src, const QString &mountPoint);
util::Error mkfs(const QString &srcDir, const QString &destImagePath);

} // namespace erofs
} // namespace linglong

#endif // LINGLONG_EROFS_H
