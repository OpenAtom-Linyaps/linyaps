// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/package/elf_handler.h"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <memory>
#include <unordered_set>

namespace linglong::package {

class UABPackager
{
public:
    explicit UABPackager(std::filesystem::path projectDir, std::filesystem::path workingDir);
    ~UABPackager() = default;

    UABPackager(UABPackager &&) = delete;

    utils::error::Result<void> setIcon(std::filesystem::path icon) noexcept;
    utils::error::Result<void> appendLayer(LayerDir layer) noexcept;
    utils::error::Result<void> pack(const std::filesystem::path &uabFilePath,
                                    bool distributedOnly) noexcept;
    utils::error::Result<void> exclude(const std::vector<std::string> &files) noexcept;
    utils::error::Result<void> include(const std::vector<std::string> &files) noexcept;
    utils::error::Result<void> loadBlackList() noexcept;
    utils::error::Result<void> loadNeededFiles() noexcept;
    void setLoader(std::filesystem::path loader) noexcept;
    void setCompressor(std::string compressor) noexcept;
    void setDefaultHeader(std::filesystem::path header) noexcept;
    void setDefaultLoader(std::filesystem::path loader) noexcept;
    void setDefaultBox(std::filesystem::path box) noexcept;
    void setBundleCB(
      std::function<utils::error::Result<void>(const std::filesystem::path &,
                                               const std::filesystem::path &)> bundleCB) noexcept;

private:
    [[nodiscard]] utils::error::Result<void> packIcon() noexcept;
    [[nodiscard]] utils::error::Result<void> packBundle(bool distributed) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareExecutableBundle(const std::filesystem::path &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareDistributedBundle(const std::filesystem::path &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void> packMetaInfo() noexcept;
    [[nodiscard]] utils::error::Result<std::pair<bool, std::unordered_set<std::string>>>
    filteringFiles(const LayerDir &layer) const noexcept;

    std::unique_ptr<ElfHandler> uab;
    std::vector<LayerDir> layers;
    std::unordered_set<std::string> excludeFiles;
    std::unordered_set<std::string> includeFiles;
    std::unordered_set<std::string> neededFiles;
    std::unordered_set<std::string> blackList;
    std::optional<std::filesystem::path> icon;
    api::types::v1::UabMetaInfo meta;
    std::filesystem::path buildDir;
    std::filesystem::path workDir;
    std::filesystem::path loader;
    std::string compressor = "lz4";
    std::filesystem::path defaultHeader;
    std::filesystem::path defaultLoader;
    std::filesystem::path defaultBox;
    std::function<utils::error::Result<void>(const std::filesystem::path &,
                                             const std::filesystem::path &)>
      bundleCB;
};
} // namespace linglong::package
