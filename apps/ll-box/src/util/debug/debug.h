/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_DEBUG_DEBUG_H_
#define LINGLONG_BOX_SRC_UTIL_DEBUG_DEBUG_H_

#include <string>

namespace linglong {

#define DUMP_FILE_INFO(path) DumpFileInfo1(path, __FUNCTION__, __LINE__)

void DumpFileInfo(const std::string &path);

void DumpFileInfo1(const std::string &path, const char *func = nullptr,
                   int line = -1);

}  // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_DEBUG_DEBUG_H_ */
