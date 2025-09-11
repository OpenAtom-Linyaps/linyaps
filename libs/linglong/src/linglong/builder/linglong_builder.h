/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/overlayfs.h"

#include <string>

namespace linglong::builder {

struct ExportOption
{
    std::string iconPath;
    std::string loader;
    std::string compressor;
    std::string ref;
    std::vector<std::string> modules;
    bool noExportDevelop{ false };
};

struct BuilderBuildOptions
{
    // 兼容选项，在制作runtime时构建全量develop, 以兼容旧版本linglong-builder使用
    // TODO 后续版本删除该选项
    bool fullDevelop{ false };
    bool skipFetchSource{ false };
    bool skipPullDepend{ false };
    bool skipRunContainer{ false };
    bool skipCommitOutput{ false };
    bool skipCheckOutput{ false };
    bool skipStripSymbols{ false };
    bool isolateNetWork{ false };
};

utils::error::Result<void> cmdListApp(repo::OSTreeRepo &repo);
utils::error::Result<void> cmdRemoveApp(repo::OSTreeRepo &repo,
                                        std::vector<std::string> refs,
                                        bool prune);

class Builder
{
public:
    // 记录linglong.yaml的位置，因为可以通过命令行参数传递，位置不再固定
    // 主要用于在构建完成后将linglong.yaml复制到应用中
    std::string projectYamlFile;
    explicit Builder(std::optional<api::types::v1::BuilderProject> project,
                     const QDir &workingDir,
                     repo::OSTreeRepo &repo,
                     runtime::ContainerBuilder &containerBuilder,
                     const api::types::v1::BuilderConfig &cfg);
    auto getConfig() const noexcept -> api::types::v1::BuilderConfig;
    void setConfig(const api::types::v1::BuilderConfig &cfg) noexcept;

    auto create(const QString &projectName) -> utils::error::Result<void>;

    auto build(const QStringList &args = { "/project/linglong/entry.sh" }) noexcept
      -> utils::error::Result<void>;

    auto exportUAB(const ExportOption &option, const std::filesystem::path &outputFile = {})
      -> utils::error::Result<void>;
    auto exportLayer(const ExportOption &option) -> utils::error::Result<void>;

    static auto extractLayer(const QString &layerPath, const QString &destination)
      -> utils::error::Result<void>;

    auto push(const std::string &module,
              const std::string &repoUrl = "",
              const std::string &repoName = "") -> utils::error::Result<void>;

    auto import() -> utils::error::Result<void>;

    static auto importLayer(repo::OSTreeRepo &repo, const QString &path)
      -> utils::error::Result<void>;

    auto run(const QStringList &modules, const QStringList &args, bool debug = false)
      -> utils::error::Result<void>;
    auto runtimeCheck() -> utils::error::Result<void>;
    auto runFromRepo(const package::Reference &ref, const std::vector<std::string> &args)
      -> utils::error::Result<void>;

    void setBuildOptions(const BuilderBuildOptions &options) noexcept { buildOptions = options; }

private:
    auto buildStagePrepare() noexcept -> utils::error::Result<void>;
    auto buildStageFetchSource() noexcept -> utils::error::Result<void>;
    utils::error::Result<void> buildStagePullDependency() noexcept;
    utils::error::Result<bool> buildStageBuild(const QStringList &args) noexcept;
    utils::error::Result<void> buildStagePreBuild() noexcept;
    utils::error::Result<void> buildStagePreCommit() noexcept;
    utils::error::Result<bool> buildStageCommit() noexcept;

    utils::error::Result<void> generateAppConf() noexcept;
    utils::error::Result<void> installFiles() noexcept;
    utils::error::Result<void> generateEntries() noexcept;
    utils::error::Result<void> processBuildDepends() noexcept;
    utils::error::Result<void> commitToLocalRepo() noexcept;
    std::unique_ptr<utils::OverlayFS> makeOverlay(QString lowerdir, QString overlayDir) noexcept;
    void fixLocaltimeInOverlay(std::unique_ptr<utils::OverlayFS> &base);
    utils::error::Result<package::Reference> ensureUtils(const std::string &id) noexcept;
    utils::error::Result<package::Reference> clearDependency(const std::string &ref,
                                                             bool forceRemote,
                                                             bool fallbackToRemote) noexcept;
    auto generateEntryScript() noexcept -> utils::error::Result<void>;
    auto generateBuildDependsScript() noexcept -> utils::error::Result<bool>;
    auto generateDependsScript() noexcept -> utils::error::Result<bool>;
    void takeTerminalForeground();
    void mergeOutput(const QList<QDir> &src, const QDir &dest, const QStringList &target);
    void printBasicInfo();
    void printRepo();

private:
    repo::OSTreeRepo &repo;
    QDir workingDir;
    std::optional<api::types::v1::BuilderProject> project;
    runtime::ContainerBuilder &containerBuilder;
    api::types::v1::BuilderConfig cfg;
    BuilderBuildOptions buildOptions;

    int64_t uid;
    int64_t gid;

    std::optional<package::Reference> projectRef;
    QStringList packageModules;
    std::unique_ptr<utils::OverlayFS> baseOverlay;
    std::unique_ptr<utils::OverlayFS> runtimeOverlay;
    QDir buildOutput;
    std::string installPrefix;
    runtime::RunContext buildContext;
};

} // namespace linglong::builder
