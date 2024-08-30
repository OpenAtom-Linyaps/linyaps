/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <string>

namespace linglong {

#define DUMP_FILESYSTEM(path) DumpFilesystem(path, __FUNCTION__, __LINE__)

#define DUMP_FILE_INFO(path) DumpFileInfo1(path, __FUNCTION__, __LINE__)

void DumpIDMap();

void DumpUidGidGroup();

void DumpFilesystem(const std::string &path, const char *func = nullptr, int line = -1);

void DumpFileInfo(const std::string &path);

void DumpFileInfo1(const std::string &path, const char *func = nullptr, int line = -1);

} // namespace linglong
