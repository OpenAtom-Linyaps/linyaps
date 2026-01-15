// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "linglong/builder/linglong_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

namespace linglong::builder {
class BuilderMock : public Builder
{
public:
    using Builder::layerExportFilename;
    using Builder::uabExportFilename;

    BuilderMock()
        : Builder(
            std::nullopt, "", initTempRepo(), initTempContainerBuilder(), initBuilderConfig()) { };

private:
    static linglong::repo::OSTreeRepo &initTempRepo()
    {
        auto tempRepoConfig = api::types::v1::RepoConfigV2{};
        char tempPath[] = "/tmp/linglong-builder-test-XXXXXX";
        std::string testDir = mkdtemp(tempPath);
        static linglong::repo::OSTreeRepo repo(QDir(testDir.c_str()), tempRepoConfig);
        return repo;
    }

    static linglong::runtime::ContainerBuilder &initTempContainerBuilder()
    {
        auto static tempCLI = ocppi::cli::crun::Crun::New(std::filesystem::current_path());
        static linglong::runtime::ContainerBuilder cb(**tempCLI);
        return cb;
    }

    static api::types::v1::BuilderConfig &initBuilderConfig()
    {
        auto static cfg = api::types::v1::BuilderConfig{ .repo = "", .version = 1 };
        return cfg;
    }
};
} // namespace linglong::builder
