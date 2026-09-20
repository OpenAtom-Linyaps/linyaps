// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "temporary_directory.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace linglong::utils {

TemporaryDirectory::TemporaryDirectory(std::filesystem::path directory)
    : directory(std::move(directory))
{
}

TemporaryDirectory::TemporaryDirectory(TemporaryDirectory &&other) noexcept
    : directory(std::exchange(other.directory, {}))
{
}

TemporaryDirectory &TemporaryDirectory::operator=(TemporaryDirectory &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    cleanup();
    directory = std::exchange(other.directory, {});
    return *this;
}

TemporaryDirectory::~TemporaryDirectory() noexcept
{
    cleanup();
}

auto TemporaryDirectory::create(std::string_view prefix, const std::filesystem::path &parent)
  -> error::Result<TemporaryDirectory>
{
    LINGLONG_TRACE("create temporary directory");

    auto pattern = (parent / (std::string(prefix) + "XXXXXX")).string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');

    const auto *created = ::mkdtemp(writablePattern.data());
    if (created == nullptr) {
        return LINGLONG_ERR("failed to create temporary directory under " + parent.string() + ": "
                            + std::string(std::strerror(errno)));
    }

    return TemporaryDirectory(created);
}

auto TemporaryDirectory::create(std::string_view prefix) -> error::Result<TemporaryDirectory>
{
    LINGLONG_TRACE("create temporary directory in the system temporary directory");

    std::error_code ec;
    auto parent = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return LINGLONG_ERR("failed to locate the system temporary directory", ec);
    }

    return create(prefix, parent);
}

std::filesystem::path TemporaryDirectory::keep() noexcept
{
    return std::exchange(directory, {});
}

void TemporaryDirectory::cleanup() noexcept
{
    if (directory.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    directory.clear();
}

} // namespace linglong::utils
