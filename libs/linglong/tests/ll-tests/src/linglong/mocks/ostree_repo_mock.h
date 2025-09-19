// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
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
    MockOstreeRepo(const QDir &path,
                   api::types::v1::RepoConfigV2 cfg,
                   repo::ClientFactory &clientFactory) noexcept
        : OSTreeRepo(path, cfg, clientFactory)
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

    // mock getOverlayShareDir
    std::function<QDir()> warpGetOverlayShareDirFunc;

protected:
    QDir getOverlayShareDir() const noexcept override
    {
        if (warpGetOverlayShareDirFunc) {
            return warpGetOverlayShareDirFunc();
        }
        return OSTreeRepo::getOverlayShareDir();
    }
};