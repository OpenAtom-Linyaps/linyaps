/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_PLATFORM_H_
#define LINGLONG_BOX_SRC_UTIL_PLATFORM_H_

#include "common.h"

#include <optional>

namespace linglong {

namespace util {

int PlatformClone(int (*callback)(void *), int flags, void *arg, ...);

int Exec(const util::str_vec &args, std::optional<std::vector<std::string>> env_list);

int Wait(const int pid);
int WaitAll();
int WaitAllUntil(const int pid);

} // namespace util

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_PLATFORM_H_ */
