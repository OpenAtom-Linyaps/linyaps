// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "file.h"

#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

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
    LINGLONG_TRACE(fmt::format("read file {}", filepath));
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
    LINGLONG_TRACE(fmt::format("write file {}", filepath));

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
            struct stat64 st{};

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
            struct stat64 st{};

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

// recursive copy src to dest with matcher
// symlinks are preserved
void copyDirectory(const std::filesystem::path &src,
                   const std::filesystem::path &dest,
                   std::function<bool(const std::filesystem::path &)> matcher,
                   std::filesystem::copy_options options)
{
    std::error_code ec;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(
           src,
           std::filesystem::directory_options::skip_permission_denied,
           ec)) {
        const auto &fromPath = entry.path();
        auto relativePath = fromPath.lexically_relative(src);

        if (matcher && !matcher(relativePath)) {
            continue;
        }

        const auto toPath = dest / relativePath;
        LogD("{} -> {}", fromPath, toPath);

        std::filesystem::create_directories(toPath.parent_path(), ec);
        if (ec) {
            LogW("failed to create directory {}: {}", toPath.parent_path(), ec.message());
            continue;
        }
        // preserve symlinks
        std::filesystem::copy(fromPath, toPath, options, ec);
        if (ec) {
            LogW("failed to copy {} to {}: {}", fromPath, toPath, ec.message());
            continue;
        }
    }
}

linglong::utils::error::Result<void>
moveFiles(const std::filesystem::path &src,
          const std::filesystem::path &dest,
          std::function<bool(const std::filesystem::path &)> matcher)
{
    LINGLONG_TRACE(fmt::format("move files from {} to {}", src, dest).c_str());

    // it must collect all files that need to be moved first, because moveing files during iteration
    // is an undefine behavior
    auto files = getFiles(src);
    if (!files) {
        return LINGLONG_ERR("failed to get files", files);
    }

    for (const auto &relPath : *files) {
        if (matcher && !matcher(relPath)) {
            continue;
        }

        const auto fromPath = src / relPath;
        const auto toPath = dest / relPath;
        LogD("{} -> {}", fromPath, toPath);

        std::error_code ec;
        auto status = std::filesystem::symlink_status(fromPath, ec);
        if (ec) {
            LogD("failed to get symlink status of {}: {}", fromPath, ec.message());
            continue;
        }
        if (!std::filesystem::exists(status)) {
            LogD("{} does not exist, skip it", fromPath);
            continue;
        }

        std::filesystem::create_directories(toPath.parent_path(), ec);
        if (ec) {
            LogW("failed to create directory {}: {}", toPath, ec.message());
            continue;
        }
        std::filesystem::rename(fromPath, toPath, ec);
        if (ec) {
            LogW("failed to copy {} to {}: {}", fromPath, toPath, ec.message());
            continue;
        }
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<std::vector<std::filesystem::path>>
getFiles(const std::filesystem::path &dir)
{
    LINGLONG_TRACE(fmt::format("get files in directory {}", dir).c_str());

    std::vector<std::filesystem::path> files;

    std::error_code ec;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(
           dir,
           std::filesystem::directory_options::skip_permission_denied,
           ec)) {
        const auto &fromPath = entry.path();
        files.emplace_back(fromPath.lexically_relative(dir));
    }

    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to iterator: {}", ec.message()).c_str());
    }

    return files;
}

linglong::utils::error::Result<void> ensureDirectory(const std::filesystem::path &dir)
{
    LINGLONG_TRACE(fmt::format("ensure directory {}", dir));

    std::error_code ec;
    auto status = std::filesystem::symlink_status(dir, ec);
    if (!ec) {
        if (std::filesystem::is_directory(status)) {
            return LINGLONG_OK;
        }

        std::filesystem::remove(dir, ec);
        if (ec) {
            return LINGLONG_ERR("failed to remove directory", ec);
        }
    }

    if (!std::filesystem::create_directories(dir, ec) && ec) {
        return LINGLONG_ERR("failed to create directory", ec);
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> relinkFileTo(const std::filesystem::path &link,
                                                  const std::filesystem::path &target) noexcept
{
    LINGLONG_TRACE(fmt::format("relink {} to {}", link.string(), target.string()));

    auto tmpPath = link.string() + ".linyaps.tmp";

    std::error_code ec;
    std::filesystem::symlink_status(tmpPath, ec);
    if (!ec) {
        std::filesystem::remove(tmpPath, ec);
    }

    std::filesystem::create_symlink(target, tmpPath, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create symlink", ec);
    }

    std::filesystem::rename(tmpPath, link, ec);
    if (ec) {
        LogW("failed to rename {} to {}: {}, fallback to remove",
             tmpPath,
             link.string(),
             ec.message());

        std::filesystem::remove_all(link, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to remove {}", link.string()), ec);
        }

        std::filesystem::rename(tmpPath, link, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to rename {} to {}", tmpPath, link.string()),
                                ec);
        }
    }

    return LINGLONG_OK;
}

} // namespace linglong::utils
