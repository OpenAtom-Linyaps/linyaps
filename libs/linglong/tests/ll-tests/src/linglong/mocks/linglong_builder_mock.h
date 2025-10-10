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
        linglong::repo::ClientFactory tempClientFactory(std::string("http://127.0.0.1"));
        auto tempRepoConfig = api::types::v1::RepoConfigV2{};
        char tempPath[] = "/tmp/linglong-builder-test-XXXXXX";
        std::string testDir = mkdtemp(tempPath);
        static linglong::repo::OSTreeRepo repo(QDir(testDir.c_str()),
                                               tempRepoConfig,
                                               tempClientFactory);
        return repo;
    }

    static linglong::runtime::ContainerBuilder &initTempContainerBuilder()
    {
        auto tempCLI = ocppi::cli::crun::Crun::New("/usr/bin/sh");
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