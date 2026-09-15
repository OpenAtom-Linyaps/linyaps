// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>

namespace linglong::ctk::detect {

utils::error::Result<std::filesystem::path> createTempDownloadDir();

utils::error::Result<std::filesystem::path> downloadDeb(const std::string &package,
                                                        const std::filesystem::path &downloadDir);

utils::error::Result<void> installDeb(const std::filesystem::path &debPath);

void cleanupTempDir(const std::filesystem::path &dir);

} // namespace linglong::ctk::detect
