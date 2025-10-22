// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <vector>

// A RAII wrapper for a temporary directory created with mkdtemp.
class TempDir
{
public:
    TempDir(const std::string &prefix = "linglong-test-")
    {
        std::string tmppath_template =
          (std::filesystem::temp_directory_path() / (prefix + "XXXXXX")).string();
        std::vector<char> tmppath_c(tmppath_template.begin(), tmppath_template.end());
        tmppath_c.push_back('\0');

        char *result = mkdtemp(tmppath_c.data());
        if (result != nullptr) {
            _path = result;
        }
    }

    ~TempDir()
    {
        if (!_path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(_path, ec);
        }
    }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;
    TempDir(TempDir &&) = delete;
    TempDir &operator=(TempDir &&) = delete;

    const std::filesystem::path &path() const { return _path; }

    bool isValid() const { return !_path.empty(); }

private:
    std::filesystem::path _path;
};
