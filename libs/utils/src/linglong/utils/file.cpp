// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "file.h"

#include "linglong/utils/error/error.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace linglong::utils {
linglong::utils::error::Result<std::string> readFile(std::string filepath)
{
    LINGLONG_TRACE(QString("read file %1").arg(filepath.c_str()));
    std::error_code ec;
    auto exists = std::filesystem::exists(filepath, ec);
    if (ec) {
        return LINGLONG_ERR("check file", ec);
    }
    if (!exists) {
        return LINGLONG_ERR("file not found");
    }
    std::ifstream in{ filepath };
    if (!in.is_open()) {
        auto msg = std::string("open file:") + std::strerror(errno);
        return LINGLONG_ERR(msg.c_str());
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    if (in.fail()) {
        auto msg = std::string("read file: ") + std::strerror(errno);
        return LINGLONG_ERR(msg.c_str());
    }
    return buffer.str();
}

linglong::utils::error::Result<void> writeFile(const std::string &filepath,
                                               const std::string &content)
{
    LINGLONG_TRACE(QString("write file %1").arg(filepath.c_str()));

    std::error_code ec;
    auto exists = std::filesystem::exists(filepath, ec);
    if (ec) {
        return LINGLONG_ERR("check file", ec);
    }
    if (exists) {
        std::filesystem::remove(filepath, ec);
        if (ec) {
            return LINGLONG_ERR("remove file", ec);
        }
    }
    std::ofstream out{ filepath };
    if (!out.is_open()) {
        auto msg = std::string("open file:") + std::strerror(errno);
        return LINGLONG_ERR(msg.c_str());
    }
    out << content;
    if (out.fail()) {
        auto msg = std::string("write file: ") + std::strerror(errno);
        return LINGLONG_ERR(msg.c_str());
    }
    return LINGLONG_OK;
}
} // namespace linglong::utils
