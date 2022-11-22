/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_SYSINFO_H_
#define LINGLONG_SRC_MODULE_UTIL_SYSINFO_H_

#include <QString>

namespace linglong {
namespace util {

QString hostArch();

QString getUserName();

} // namespace util
} // namespace linglong
#endif
