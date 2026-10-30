// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "builder_directory.h"

#include "linglong/package/architecture.h"
#include "linglong/utils/file.h"

#include <fmt/format.h>

#include <utility>

namespace linglong::builder {

auto getProjectYamlPath(const std::filesystem::path &projectDir,
                        const std::filesystem::path &usePath) noexcept
  -> utils::error::Result<std::filesystem::path>
{
    LINGLONG_TRACE("get project yaml path");

    std::error_code ec;
    auto path = usePath;
    if (path.empty()) {
        path = projectDir
          / ("linglong." + package::Architecture::currentCPUArchitecture().toString() + ".yaml");
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            return LINGLONG_ERR(fmt::format("path {} error: {}", path.string(), ec.message()));
        }
        if (!exists) {
            path = projectDir / "linglong.yaml";
        }
    }

    // Resolve directory aliases, but keep a YAML file symlink as the project entry point.
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    const auto directory = std::filesystem::canonical(parent, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("invalid file path {} error: {}", path.string(), ec.message()));
    }
    path = directory / path.filename();

    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("path {} error: {}", path.string(), ec.message()));
    }
    if (!exists) {
        return LINGLONG_ERR("project yaml file not found: " + path.string());
    }

    return path;
}

const std::filesystem::path BuilderDirectoryLayout::containerProject = "/project";
const std::filesystem::path BuilderDirectoryLayout::containerRoot = containerProject / "linglong";
const std::filesystem::path BuilderDirectoryLayout::containerOutput = containerRoot / "output";
const std::filesystem::path BuilderDirectoryLayout::containerOverlays = containerRoot / "overlay";
const std::filesystem::path BuilderDirectoryLayout::containerEntryScript =
  containerRoot / "entry.sh";
const std::filesystem::path BuilderDirectoryLayout::containerBuildExtScript =
  containerRoot / "buildext.sh";

BuilderDirectoryLayout::BuilderDirectoryLayout(std::filesystem::path project)
    : projectRoot(std::move(project))
    , stateRoot(projectRoot / "linglong")
    , cacheRoot(stateRoot / "cache")
    , sourceRoot(stateRoot / "sources")
    , outputRoot(stateRoot / "output")
    , buildOutputRoot(outputRoot / "_build")
    , overlayRoot(stateRoot / "overlay")
    , buildBaseOverlayPath(overlayRoot / "build_base")
    , buildRuntimeOverlayPath(overlayRoot / "build_runtime")
    , prepareBaseOverlayPath(overlayRoot / "prepare_base")
    , prepareRuntimeOverlayPath(overlayRoot / "prepare_runtime")
    , buildOverlayStatePath(overlayRoot / "build_overlay.state")
    , entryScriptPath(stateRoot / "entry.sh")
    , buildExtScriptPath(stateRoot / "buildext.sh")
{
}

auto BuilderDirectoryLayout::module(std::string_view name) const -> std::filesystem::path
{
    return outputRoot / name;
}

auto BuilderDirectoryLayout::lockProject() const noexcept
  -> utils::error::Result<utils::filelock::FileLock>
{
    LINGLONG_TRACE("lock builder project");

    std::error_code ec;
    const auto project = std::filesystem::canonical(projectRoot, ec);
    if (ec) {
        return LINGLONG_ERR("failed to resolve project directory: " + ec.message());
    }

    // Keep the lock outside the directories removed by clean, and never unlink it.
    auto lock = utils::filelock::FileLock::create(project / ".ll-builder.lock",
                                                  utils::filelock::LockType::Write);
    if (!lock) {
        return LINGLONG_ERR("failed to create project lock", lock);
    }

    auto locked = lock->tryLock(utils::filelock::LockType::Write);
    if (!locked) {
        return LINGLONG_ERR("failed to acquire project lock", locked);
    }
    if (!*locked) {
        return LINGLONG_ERR("project is already in use by another ll-builder process: "
                            + project.string());
    }

    return lock;
}

auto BuilderDirectoryLayout::ensureRoot() const noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("ensure builder state directory");

    std::error_code ec;
    std::filesystem::create_directories(stateRoot, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to create directory {}: {}", stateRoot.string(), ec.message()));
    }

    return LINGLONG_OK;
}

auto BuilderDirectoryLayout::resetSources() const noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("reset builder source directory");

    return reset(sourceRoot, true);
}

auto BuilderDirectoryLayout::resetOutput() const noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("reset builder output directory");

    auto result = reset(outputRoot, false);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    std::error_code ec;
    std::filesystem::create_directories(buildOutputRoot, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to create directory {}: {}", buildOutputRoot.string(), ec.message()));
    }

    return LINGLONG_OK;
}

auto BuilderDirectoryLayout::resetOverlay(const std::filesystem::path &directory,
                                          bool createUpperdir) const noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("reset builder overlay directory");

    auto result = reset(directory, false);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    if (createUpperdir) {
        std::error_code ec;
        std::filesystem::create_directories(directory / "upperdir", ec);
        if (ec) {
            return LINGLONG_ERR("failed to create overlay: " + ec.message());
        }
    }

    return LINGLONG_OK;
}

auto BuilderDirectoryLayout::clean() const noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("clean builder state directory");

    std::error_code ec;
    const auto exists = std::filesystem::exists(stateRoot, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to check directory {}: {}", stateRoot.string(), ec.message()));
    }
    if (!exists) {
        return LINGLONG_OK;
    }

    auto removable = utils::makeDirectoryTreeRemovable(stateRoot);
    if (!removable) {
        return LINGLONG_ERR("failed to make directory tree removable", removable);
    }

    std::filesystem::remove_all(stateRoot, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to remove {}: {}", stateRoot.string(), ec.message()));
    }

    return LINGLONG_OK;
}

auto BuilderDirectoryLayout::reset(const std::filesystem::path &directory,
                                   bool recreate) const noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("reset builder directory");

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to reset directory {}: {}", directory.string(), ec.message()));
    }

    if (!recreate) {
        return LINGLONG_OK;
    }

    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to create directory {}: {}", directory.string(), ec.message()));
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
