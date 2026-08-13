// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/package/elf_handler.h"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace linglong::package {

enum class UABPackagerMode {
    Distribution,
    Exec,
};

namespace detail {

[[nodiscard]] utils::error::Result<void> copyDirectoryForDistributedBundle(
  const std::filesystem::path &source, const std::filesystem::path &destination) noexcept;
[[nodiscard]] utils::error::Result<std::string> generateExecEntry(
  const std::vector<std::string> &command, const std::filesystem::path &prefix) noexcept;
[[nodiscard]] utils::error::Result<void>
ensureExecEntry(const std::filesystem::path &entryPath,
                const std::optional<std::vector<std::string>> &command,
                const std::filesystem::path &prefix) noexcept;
[[nodiscard]] std::string generateExecLoader() noexcept;

} // namespace detail

class UABPackager
{
public:
    explicit UABPackager(std::filesystem::path workingDir);
    ~UABPackager() = default;

    UABPackager(UABPackager &&) = delete;

    utils::error::Result<void> setIcon(std::filesystem::path icon) noexcept;
    utils::error::Result<void> appendLayer(LayerDir layer) noexcept;
    utils::error::Result<void> pack(const std::filesystem::path &uabFilePath,
                                    UABPackagerMode mode) noexcept;
    void setLoader(std::filesystem::path loader) noexcept;
    void setCompressor(std::string compressor) noexcept;
    void setDefaultHeader(std::filesystem::path header) noexcept;
    void setBundleCB(
      std::function<utils::error::Result<void>(const std::filesystem::path &,
                                               const std::filesystem::path &)> bundleCB) noexcept;

private:
    [[nodiscard]] utils::error::Result<void> packIcon() noexcept;
    [[nodiscard]] utils::error::Result<void> packBundle(UABPackagerMode mode) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareExecutableBundle(const std::filesystem::path &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareDistributedBundle(const std::filesystem::path &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void> packMetaInfo() noexcept;
    std::unique_ptr<ElfHandler> uab;
    std::vector<LayerDir> layers;
    std::optional<std::filesystem::path> icon;
    api::types::v1::UabMetaInfo meta;
    std::filesystem::path buildDir;
    std::filesystem::path loader;
    std::string compressor = "lz4";
    std::filesystem::path defaultHeader;
    std::function<utils::error::Result<void>(const std::filesystem::path &,
                                             const std::filesystem::path &)>
      bundleCB;
};
} // namespace linglong::package
