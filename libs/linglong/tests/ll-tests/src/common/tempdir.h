// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/temporary_directory.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

// A test-friendly wrapper around the production temporary directory utility.
class TempDir
{
public:
    TempDir(const std::string &prefix = "linglong-test-")
        : directory_(create(prefix))
    {
    }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;
    TempDir(TempDir &&) = delete;
    TempDir &operator=(TempDir &&) = delete;

    const std::filesystem::path &path() const { return directory_.path(); }

private:
    static linglong::utils::TemporaryDirectory create(const std::string &prefix)
    {
        auto directory = linglong::utils::TemporaryDirectory::create(prefix);
        if (!directory) {
            throw std::runtime_error("failed to create test temporary directory: "
                                     + directory.error().message());
        }

        return std::move(*directory);
    }

    linglong::utils::TemporaryDirectory directory_;
};
