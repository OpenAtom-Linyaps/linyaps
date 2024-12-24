// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include "linglong/utils/error/error.h"

#include <string>

namespace linglong::utils {
linglong::utils::error::Result<std::string> readFile(std::string filepath);

linglong::utils::error::Result<void> writeFile(const std::string &filepath,
                                               const std::string &content);
} // namespace linglong::utils
