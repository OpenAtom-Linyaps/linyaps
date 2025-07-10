// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <memory>
#include <filesystem>
#include <vector>

namespace linglong::package {

class ElfHandler
{
public:
    ElfHandler(std::filesystem::path file);
    ~ElfHandler();
    static utils::error::Result<std::unique_ptr<ElfHandler>> create(std::filesystem::path file);

    utils::error::Result<void> addSection(const std::string &name, const char *data, size_t size);
    utils::error::Result<void> addSection(const std::string &name, const std::filesystem::path &file);

private:

    std::filesystem::path file_;
};

}
