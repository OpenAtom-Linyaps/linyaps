// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/client_factory.h"
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

    utils::error::Result<void>
    exportLayerEntries(const std::filesystem::path &destination,
                       const api::types::v1::RepositoryCacheLayersItem &item)
    {
        return this->OSTreeRepo::exportLayerEntries(destination, item);
    }

    utils::error::Result<void>
    exportAppEntries(const std::filesystem::path &destination,
                     const api::types::v1::RepositoryCacheLayersItem &item)
    {
        return this->OSTreeRepo::exportAppEntries(destination, item);
    }

    utils::error::Result<void>
    exportAppBinaries(const std::filesystem::path &destination,
                      const api::types::v1::RepositoryCacheLayersItem &item)
    {
        return this->OSTreeRepo::exportAppBinaries(destination, item);
    }

    utils::error::Result<void>
    exportLayerSignData(const std::filesystem::path &destination,
                        const api::types::v1::RepositoryCacheLayersItem &item)
    {
        return this->OSTreeRepo::exportLayerSignData(destination, item);
    }

    utils::error::Result<void>
    unexportLayerSignData(const std::filesystem::path &destination,
                          const api::types::v1::RepositoryCacheLayersItem &item)
    {
        return this->OSTreeRepo::unexportLayerSignData(destination, item);
    }

    utils::error::Result<void> unexportAppEntries(
      const std::filesystem::path &destination, const std::vector<std::filesystem::path> &layerDirs)
    {
        return this->OSTreeRepo::unexportAppEntries(destination, layerDirs);
    }

    // mock getOverlayShareDir
    std::function<std::filesystem::path()> wrapGetOverlayShareDirFunc;
    std::function<utils::error::Result<bool>()> wrapShouldExportSignDataFunc;

protected:
    utils::error::Result<bool> shouldExportSignData() const noexcept override
    {
        if (wrapShouldExportSignDataFunc) {
            return wrapShouldExportSignDataFunc();
        }
        return OSTreeRepo::shouldExportSignData();
    }

    std::filesystem::path getOverlayShareDir() const noexcept override
    {
        if (wrapGetOverlayShareDirFunc) {
            return wrapGetOverlayShareDirFunc();
        }
        return OSTreeRepo::getOverlayShareDir();
    }
};
