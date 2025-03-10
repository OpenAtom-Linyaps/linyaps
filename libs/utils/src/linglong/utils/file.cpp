// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "file.h"

#include "linglong/utils/error/error.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <sys/stat.h>

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

linglong::utils::error::Result<uintmax_t>
calculateDirectorySize(const std::filesystem::path &dir) noexcept
{
    LINGLONG_TRACE("calculate directory size")

    uintmax_t size{ 0 };
    std::error_code ec;

    auto fsIter = std::filesystem::recursive_directory_iterator{ dir, ec };
    if (ec) {
        return LINGLONG_ERR(
          QString{ "failed to calculate directory size: %1" }.arg(ec.message().c_str()));
    }

    for (const auto &entry : fsIter) {
        auto path = entry.path().string();
        if (entry.is_symlink(ec)) {
            struct stat64 st
            {
            };

            if (::lstat64(path.c_str(), &st) == -1) {
                return LINGLONG_ERR(
                  QString{ "failed to get symlink size: %1" }.arg(::strerror(errno)));
            }

            size += st.st_size;
            continue;
        }
        if (ec) {
            return LINGLONG_ERR(
              QString{ "failed to get entry type of %1: %2" }.arg(entry.path().c_str(),
                                                                  ec.message().c_str()));
        }

        if (entry.is_directory(ec)) {
            struct stat64 st
            {
            };

            if (::stat64(path.c_str(), &st) == -1) {
                return LINGLONG_ERR(
                  QString{ "failed to get directory size: %1" }.arg(::strerror(errno)));
            }
            size += st.st_size;
            continue;
        }
        if (ec) {
            return LINGLONG_ERR(
              QString{ "failed to get entry type of %1: %2" }.arg(entry.path().c_str(),
                                                                  ec.message().c_str()));
        }

        size += entry.file_size();
    }

    return size;
}
} // namespace linglong::utils
