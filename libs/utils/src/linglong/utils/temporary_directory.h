// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string_view>

namespace linglong::utils {

class TemporaryDirectory
{
public:
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&other) noexcept;
    TemporaryDirectory &operator=(TemporaryDirectory &&other) noexcept;
    ~TemporaryDirectory() noexcept;

    static auto create(std::string_view prefix) -> error::Result<TemporaryDirectory>;
    static auto create(std::string_view prefix, const std::filesystem::path &parent)
      -> error::Result<TemporaryDirectory>;

    [[nodiscard]] const std::filesystem::path &path() const noexcept { return directory; }

    // Stops automatic removal and returns the directory path to the caller.
    [[nodiscard]] std::filesystem::path keep() noexcept;

private:
    explicit TemporaryDirectory(std::filesystem::path directory);
    void cleanup() noexcept;

    std::filesystem::path directory;
};

} // namespace linglong::utils
