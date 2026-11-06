// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/runtime_directory.h"

#include "linglong/common/error.h"
#include "linglong/common/xdg.h"

#include <fmt/format.h>

#include <cerrno>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong::utils::runtime_directory {

namespace {

constexpr auto privateDirectoryMode = S_IRWXU;
constexpr auto permissionModeMask = 07777;
// nsfs exposes the initial user namespace with this reserved inode number.
constexpr ino_t initialUserNamespaceInode = 0xEFFFFFFD;

enum class ExistingDirectoryMode {
    RequirePrivate,
    TightenPrivate,
};

utils::error::Result<struct stat> inspectDirectory(const std::filesystem::path &directory) noexcept
{
    LINGLONG_TRACE("inspect runtime directory");

    struct stat st{};
    if (::lstat(directory.c_str(), &st) == -1) {
        return LINGLONG_ERR(fmt::format("failed to inspect {}: {}",
                                        directory.c_str(),
                                        common::error::errorString(errno)));
    }

    if (!S_ISDIR(st.st_mode)) {
        return LINGLONG_ERR(fmt::format("{} is not a directory", directory.c_str()));
    }

    return st;
}

utils::error::Result<void> validateOwner(const struct stat &st,
                                         const std::filesystem::path &directory,
                                         uid_t owner) noexcept
{
    LINGLONG_TRACE("validate runtime directory owner");

    if (st.st_uid != owner) {
        return LINGLONG_ERR(
          fmt::format("{} is owned by uid {}, expected {}", directory.c_str(), st.st_uid, owner));
    }

    return LINGLONG_OK;
}

utils::error::Result<bool> isInitialUserNamespace() noexcept
{
    LINGLONG_TRACE("check initial user namespace");

    const auto userNamespace = ::open("/proc/self/ns/user", O_RDONLY | O_CLOEXEC);
    if (userNamespace == -1) {
        return LINGLONG_ERR(
          fmt::format("failed to open user namespace: {}", common::error::errorString(errno)));
    }

    struct stat st{};
    if (::fstat(userNamespace, &st) == -1) {
        const auto statErrno = errno;
        ::close(userNamespace);
        return LINGLONG_ERR(
          fmt::format("failed to stat user namespace: {}", common::error::errorString(statErrno)));
    }
    ::close(userNamespace);

    return initialUserNamespaceInode == st.st_ino;
}

utils::error::Result<void> validateSafeDirectory(const std::filesystem::path &directory,
                                                 uid_t owner) noexcept
{
    LINGLONG_TRACE("validate safe runtime directory");

    auto inspectResult = inspectDirectory(directory);
    if (!inspectResult) {
        return LINGLONG_ERR(inspectResult);
    }
    const auto &st = *inspectResult;

    auto ownerResult = validateOwner(st, directory, owner);
    if (!ownerResult) {
        return LINGLONG_ERR(ownerResult);
    }

    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        return LINGLONG_ERR(fmt::format("{} is writable by group or others", directory.c_str()));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> ensurePrivateDirectory(const std::filesystem::path &directory,
                                                  uid_t owner,
                                                  ExistingDirectoryMode mode) noexcept
{
    LINGLONG_TRACE("ensure private runtime directory");

    // Path-based checks assume the parent cannot be replaced by untrusted users.
    // Privileged callers enforce that precondition. Supporting an untrusted
    // parent would require dirfd-based operations.
    if (::mkdir(directory.c_str(), privateDirectoryMode) == -1 && errno != EEXIST) {
        return LINGLONG_ERR(fmt::format("failed to create {}: {}",
                                        directory.c_str(),
                                        common::error::errorString(errno)));
    }
    auto inspectResult = inspectDirectory(directory);
    if (!inspectResult) {
        return LINGLONG_ERR(inspectResult);
    }
    const auto &st = *inspectResult;

    auto result = validateOwner(st, directory, owner);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    if ((st.st_mode & permissionModeMask) != privateDirectoryMode) {
        if (mode == ExistingDirectoryMode::RequirePrivate) {
            return LINGLONG_ERR(fmt::format("{} must have permissions 0700, found {:04o}",
                                            directory.c_str(),
                                            st.st_mode & permissionModeMask));
        }

        if (::chmod(directory.c_str(), privateDirectoryMode) == -1) {
            return LINGLONG_ERR(fmt::format("failed to set permissions for {}: {}",
                                            directory.c_str(),
                                            common::error::errorString(errno)));
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> preparePrivilegedRuntimeDir(const std::filesystem::path &runtimeDir,
                                                       uid_t owner) noexcept
{
    LINGLONG_TRACE("prepare privileged runtime directory");

    const auto parent = runtimeDir.parent_path();
    auto result = validateSafeDirectory(parent, owner);
    if (!result) {
        return LINGLONG_ERR(
          fmt::format("privileged runtime requires an existing safe parent {}", parent.c_str()),
          result);
    }

    return ensurePrivateDirectory(runtimeDir, owner, ExistingDirectoryMode::RequirePrivate);
}

utils::error::Result<std::filesystem::path> prepareAppRuntimeTree(
  const std::filesystem::path &runtimeDir, const std::string &appId, uid_t owner) noexcept
{
    LINGLONG_TRACE("prepare app runtime tree");

    const auto appRuntimeDir = runtimeDir / "linglong" / "apps" / appId;
    for (const auto &directory :
         { runtimeDir / "linglong", runtimeDir / "linglong" / "apps", appRuntimeDir }) {
        auto result =
          ensurePrivateDirectory(directory, owner, ExistingDirectoryMode::TightenPrivate);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    return appRuntimeDir;
}

} // namespace

RuntimeLayout::RuntimeLayout(std::filesystem::path root, bool privilegedHost) noexcept
    : root_(std::move(root))
    , privilegedHost_(privilegedHost)
{
}

RuntimeLayout RuntimeLayout::forUser(std::filesystem::path root) noexcept
{
    return RuntimeLayout(std::move(root), false);
}

RuntimeLayout RuntimeLayout::forPrivilegedHost(std::filesystem::path root) noexcept
{
    return RuntimeLayout(std::move(root), true);
}

utils::error::Result<RuntimeLayout> RuntimeLayout::current() noexcept
{
    LINGLONG_TRACE("resolve current runtime layout");

    if (::geteuid() != 0) {
        return forUser(common::xdg::getXDGRuntimeDir());
    }

    auto initialNamespace = isInitialUserNamespace();
    if (!initialNamespace) {
        return LINGLONG_ERR(initialNamespace);
    }

    if (*initialNamespace) {
        return forPrivilegedHost(common::xdg::getXDGRuntimeDirForUid(::geteuid()));
    }

    const auto *xdgRuntimeDir = ::getenv("XDG_RUNTIME_DIR");
    if (xdgRuntimeDir == nullptr || xdgRuntimeDir[0] == '\0') {
        return LINGLONG_ERR("XDG_RUNTIME_DIR is required inside a user namespace");
    }

    return forUser(xdgRuntimeDir);
}

const std::filesystem::path &RuntimeLayout::root() const noexcept
{
    return root_;
}

std::filesystem::path RuntimeLayout::runtimeDir() const noexcept
{
    return root_ / "linglong";
}

bool RuntimeLayout::isPrivilegedHost() const noexcept
{
    return privilegedHost_;
}

std::filesystem::path RuntimeLayout::bundleDir(std::string_view containerID,
                                               std::string_view bundleSuffix) const noexcept
{
    auto bundle = runtimeDir() / containerID;
    if (!bundleSuffix.empty()) {
        bundle += bundleSuffix;
    }
    return bundle;
}

std::filesystem::path RuntimeLayout::appRuntimeDir(const std::string &appId) const noexcept
{
    return runtimeDir() / "apps" / appId;
}

std::filesystem::path RuntimeLayout::containerStateDir() const noexcept
{
    return runtimeDir() / "box";
}

std::filesystem::path RuntimeLayout::consoleSocket(std::string_view name) const noexcept
{
    return runtimeDir() / name;
}

utils::error::Result<void> RuntimeLayout::prepareRuntimeDir() const noexcept
{
    LINGLONG_TRACE("prepare runtime directory");

    if (privilegedHost_) {
        return preparePrivilegedRuntimeDir(root_, ::geteuid());
    }

    std::error_code ec;
    std::filesystem::create_directories(runtimeDir(), ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to create {}", runtimeDir().c_str()), ec);
    }

    return LINGLONG_OK;
}

utils::error::Result<std::filesystem::path>
RuntimeLayout::prepareAppRuntimeDir(const std::string &appId) const noexcept
{
    LINGLONG_TRACE("prepare layout app runtime directory");

    auto result = validateAppId(appId);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    if (privilegedHost_) {
        return preparePrivilegedAppRuntimeDir(root_, appId, ::geteuid());
    }

    const auto appRuntimeDir = this->appRuntimeDir(appId);
    std::error_code ec;
    std::filesystem::create_directories(appRuntimeDir, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create directories for container XDG_RUNTIME_DIR", ec);
    }

    std::filesystem::permissions(appRuntimeDir, std::filesystem::perms::owner_all, ec);
    if (ec) {
        return LINGLONG_ERR("failed to set permissions for container XDG_RUNTIME_DIR", ec);
    }

    return appRuntimeDir;
}

utils::error::Result<void> validateAppId(const std::string &appId) noexcept
{
    LINGLONG_TRACE("validate app id");

    if (appId.empty()) {
        return LINGLONG_ERR("app id is empty");
    }
    if (appId == "." || appId == "..") {
        return LINGLONG_ERR(fmt::format("invalid app id: {}", appId));
    }
    if (appId.find('/') != std::string::npos || appId.find('\\') != std::string::npos
        || appId.find('\0') != std::string::npos) {
        return LINGLONG_ERR(fmt::format("invalid app id: {}", appId));
    }

    return LINGLONG_OK;
}

utils::error::Result<std::filesystem::path> preparePrivilegedAppRuntimeDir(
  const std::filesystem::path &runtimeDir, const std::string &appId, uid_t owner) noexcept
{
    LINGLONG_TRACE("prepare privileged app runtime directory");

    auto result = validateAppId(appId);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    result = preparePrivilegedRuntimeDir(runtimeDir, owner);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return prepareAppRuntimeTree(runtimeDir, appId, owner);
}

} // namespace linglong::utils::runtime_directory
