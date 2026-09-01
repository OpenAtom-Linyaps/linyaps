// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"

#include <filesystem>
#include <string_view>

namespace linglong::builder {

auto getProjectYamlPath(const std::filesystem::path &projectDir,
                        const std::filesystem::path &usePath) noexcept
  -> utils::error::Result<std::filesystem::path>;

class BuilderDirectoryLayout
{
public:
    explicit BuilderDirectoryLayout(std::filesystem::path projectRoot);

    const std::filesystem::path &project() const noexcept { return projectRoot; }

    const std::filesystem::path &root() const noexcept { return stateRoot; }

    const std::filesystem::path &cache() const noexcept { return cacheRoot; }

    const std::filesystem::path &sources() const noexcept { return sourceRoot; }

    const std::filesystem::path &output() const noexcept { return outputRoot; }

    const std::filesystem::path &buildOutput() const noexcept { return buildOutputRoot; }

    const std::filesystem::path &overlays() const noexcept { return overlayRoot; }

    const std::filesystem::path &buildBaseOverlay() const noexcept { return buildBaseOverlayPath; }

    const std::filesystem::path &buildRuntimeOverlay() const noexcept
    {
        return buildRuntimeOverlayPath;
    }

    const std::filesystem::path &prepareBaseOverlay() const noexcept
    {
        return prepareBaseOverlayPath;
    }

    const std::filesystem::path &prepareRuntimeOverlay() const noexcept
    {
        return prepareRuntimeOverlayPath;
    }

    const std::filesystem::path &buildOverlayState() const noexcept
    {
        return buildOverlayStatePath;
    }

    const std::filesystem::path &entryScript() const noexcept { return entryScriptPath; }

    const std::filesystem::path &buildExtScript() const noexcept { return buildExtScriptPath; }

    auto module(std::string_view name) const -> std::filesystem::path;

    auto lockProject() const noexcept -> utils::error::Result<utils::filelock::FileLock>;
    auto ensureRoot() const noexcept -> utils::error::Result<void>;
    auto resetSources() const noexcept -> utils::error::Result<void>;
    auto resetOutput() const noexcept -> utils::error::Result<void>;
    auto resetOverlay(const std::filesystem::path &directory,
                      bool createUpperdir = false) const noexcept -> utils::error::Result<void>;
    auto clean() const noexcept -> utils::error::Result<void>;

    static const std::filesystem::path containerProject;
    static const std::filesystem::path containerRoot;
    static const std::filesystem::path containerOutput;
    static const std::filesystem::path containerOverlays;
    static const std::filesystem::path containerEntryScript;
    static const std::filesystem::path containerBuildExtScript;

private:
    auto reset(const std::filesystem::path &directory, bool recreate) const noexcept
      -> utils::error::Result<void>;

    std::filesystem::path projectRoot;
    std::filesystem::path stateRoot;
    std::filesystem::path cacheRoot;
    std::filesystem::path sourceRoot;
    std::filesystem::path outputRoot;
    std::filesystem::path buildOutputRoot;
    std::filesystem::path overlayRoot;
    std::filesystem::path buildBaseOverlayPath;
    std::filesystem::path buildRuntimeOverlayPath;
    std::filesystem::path prepareBaseOverlayPath;
    std::filesystem::path prepareRuntimeOverlayPath;
    std::filesystem::path buildOverlayStatePath;
    std::filesystem::path entryScriptPath;
    std::filesystem::path buildExtScriptPath;
};

} // namespace linglong::builder
