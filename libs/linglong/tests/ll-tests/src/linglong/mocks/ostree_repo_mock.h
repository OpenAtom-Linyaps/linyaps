// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>

using namespace linglong;

class MockOstreeRepo : public repo::OSTreeRepo
{
public:
    MockOstreeRepo(const std::filesystem::path &path, api::types::v1::RepoConfigV2 cfg) noexcept
        : OSTreeRepo(path, cfg)
    {
    }

    // 公开exportDir以便测试
    utils::error::Result<void> exportDir(const std::string &appID,
                                         const std::filesystem::path &source,
                                         const std::filesystem::path &destination,
                                         const int &max_depth)
    {
        return this->OSTreeRepo::exportDir(appID, source, destination, max_depth);
    }

    utils::error::Result<void> exportEntries(
      const std::filesystem::path &rootEntriesDir,
      const api::types::v1::RepositoryCacheLayersItem &item,
      const std::optional<std::vector<std::string>> &exportPathsFilter = std::nullopt) noexcept
    {
        return this->OSTreeRepo::exportEntries(rootEntriesDir, item, exportPathsFilter);
    }

    // mock getOverlayShareDir
    std::function<std::filesystem::path()> wrapGetOverlayShareDirFunc;

protected:
    std::filesystem::path getOverlayShareDir() const noexcept override
    {
        if (wrapGetOverlayShareDirFunc) {
            return wrapGetOverlayShareDirFunc();
        }
        return OSTreeRepo::getOverlayShareDir();
    }
};
