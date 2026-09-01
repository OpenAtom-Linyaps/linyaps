/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/builder/builder_directory.h"
#include "linglong/builder/project_lock.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/overlayfs.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace linglong::builder {

enum class ExportMode {
    Distribution,
    Exec,
};

struct ExportOption
{
    std::string iconPath;
    std::string loader;
    std::string compressor;
    std::string ref;
    std::vector<std::string> modules;
    bool noExportDevelop{ false };
    ExportMode mode{ ExportMode::Distribution };
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
    bool locked{ false };
    bool offline{ false };
};

utils::error::Result<std::vector<std::filesystem::path>>
installModule(const std::filesystem::path &buildOutput,
              const std::filesystem::path &moduleOutput,
              const std::unordered_set<std::string> &rules);
utils::error::Result<void> cmdListApp(repo::OSTreeRepo &repo);
utils::error::Result<void> cmdRemoveApp(repo::OSTreeRepo &repo,
                                        std::vector<std::string> refs,
                                        bool prune);

namespace detail {
utils::error::Result<void> mergeOutput(const std::vector<std::filesystem::path> &src,
                                       const std::filesystem::path &dest,
                                       const std::vector<std::string> &targets,
                                       const std::vector<std::string> &excludes);
utils::error::Result<void> pullResolvedRef(const package::ReferenceWithRepo &refRepo,
                                           repo::OSTreeRepo &repo,
                                           const std::string &module) noexcept;
using DependencyReference =
  std::pair<std::optional<package::ReferenceWithRepo>, std::optional<package::Reference>>;
utils::error::Result<DependencyReference>
clearDependency(const std::string &fuzzyRefStr,
                repo::OSTreeRepo &repo,
                bool useRemote,
                std::optional<std::string> module = std::nullopt) noexcept;
utils::error::Result<package::Reference> pullDependency(const std::string &fuzzyRefStr,
                                                        repo::OSTreeRepo &repo,
                                                        const std::string &module) noexcept;
auto prepareBuildOverlays(const BuilderDirectoryLayout &directories,
                          const std::string &dependencyState,
                          bool hasRuntime) -> utils::error::Result<bool>;
} // namespace detail

class Builder
{
public:
    // 记录linglong.yaml的位置，因为可以通过命令行参数传递，位置不再固定
    // 主要用于在构建完成后将linglong.yaml复制到应用中
    std::string projectYamlFile;
    explicit Builder(std::optional<api::types::v1::BuilderProject> project,
                     std::filesystem::path workingDir,
                     repo::OSTreeRepo &repo,
                     runtime::ContainerBuilder &containerBuilder,
                     const api::types::v1::BuilderConfig &cfg);
    auto getConfig() const noexcept -> api::types::v1::BuilderConfig;
    void setConfig(const api::types::v1::BuilderConfig &cfg) noexcept;

    auto create(const QString &projectName) -> utils::error::Result<void>;

    auto build(std::vector<std::string> args = {}) noexcept -> utils::error::Result<void>;

    auto processProjectLock(ProjectLockMode mode) noexcept -> utils::error::Result<void>;

    auto exportUAB(const ExportOption &option, const std::filesystem::path &outputFile = {})
      -> utils::error::Result<void>;
    auto exportLayer(const ExportOption &option) -> utils::error::Result<void>;

    static auto extractLayer(const QString &layerPath, const QString &destination)
      -> utils::error::Result<void>;

    auto push(const std::string &module,
              const std::string &repoUrl = "",
              const std::string &repoName = "") -> utils::error::Result<void>;

    auto import() -> utils::error::Result<void>;

    static auto importLayer(repo::OSTreeRepo &repo, const std::filesystem::path &path)
      -> utils::error::Result<void>;

    auto run(std::vector<std::string> modules,
             std::vector<std::string> args,
             bool debug = false,
             const std::string &workdir = "",
             std::vector<std::string> extensions = {}) -> utils::error::Result<void>;
    auto runtimeCheck() -> utils::error::Result<void>;

    void setBuildOptions(const BuilderBuildOptions &options) noexcept { buildOptions = options; }

    auto cleanBuildArtifacts() noexcept -> utils::error::Result<void>;

protected:
    std::string uabExportFilename(const linglong::package::Reference &ref, ExportMode mode);
    std::string layerExportFilename(const linglong::package::Reference &ref,
                                    const std::string &module);
    auto buildStageFetchSource() noexcept -> utils::error::Result<void>;

private:
    auto lockProject() noexcept -> utils::error::Result<void>;
    auto buildStagePrepare() noexcept -> utils::error::Result<void>;
    auto sourceCachePath() const -> std::filesystem::path;
    utils::error::Result<void> buildStagePullDependency() noexcept;
    auto fetchProjectDependency(const DependencyFetchRequest &request) noexcept
      -> utils::error::Result<package::Reference>;
    auto fetchResolvedProjectDependency(const ResolveDependencyRequest &request) noexcept
      -> utils::error::Result<package::Reference>;
    auto ensureDependencyModule(const package::Reference &reference,
                                const std::string &module,
                                std::optional<package::ReferenceWithRepo> &remote) noexcept
      -> utils::error::Result<void>;
    auto recordProjectDependency(const DependencyFetchRequest &request,
                                 const package::Reference &reference) noexcept
      -> utils::error::Result<LockedDependency>;
    auto finishProjectLockDependencies(const ProjectDependencyPlan &plan,
                                       const package::Reference &base,
                                       const std::optional<package::Reference> &runtime) noexcept
      -> utils::error::Result<void>;
    utils::error::Result<bool> buildStageBuild(std::vector<std::string> args) noexcept;
    utils::error::Result<void> buildStagePreBuild() noexcept;
    utils::error::Result<void> buildStagePreCommit() noexcept;
    utils::error::Result<bool> buildStageCommit() noexcept;

    utils::error::Result<void> generateAppConf() noexcept;
    utils::error::Result<void> installFiles() noexcept;
    utils::error::Result<void> generateEntries() noexcept;
    utils::error::Result<void> processBuildDepends() noexcept;
    utils::error::Result<void> commitToLocalRepo() noexcept;
    std::unique_ptr<utils::OverlayFS> makeOverlay(const std::filesystem::path &lowerdir,
                                                  const std::filesystem::path &overlayDir) noexcept;
    void fixLocaltimeInOverlay(std::unique_ptr<utils::OverlayFS> &base);
    utils::error::Result<package::Reference>
    ensureUtils(const std::string &id, const package::Architecture &arch) noexcept;
    auto runFromRepo(const package::Reference &ref, const std::vector<std::string> &args)
      -> utils::error::Result<void>;
    auto generateEntryScript() noexcept -> utils::error::Result<void>;
    auto generateBuildDependsScript() noexcept -> utils::error::Result<bool>;
    auto generateDependsScript() noexcept -> utils::error::Result<bool>;
    void takeTerminalForeground();
    void printBasicInfo();
    void printRepo();
    bool checkDeprecatedInstallFile();

private:
    repo::OSTreeRepo &repo;
    BuilderDirectoryLayout directories;
    std::optional<api::types::v1::BuilderProject> project;
    runtime::ContainerBuilder &containerBuilder;
    api::types::v1::BuilderConfig cfg;
    BuilderBuildOptions buildOptions;
    // Acquired lazily and kept until this Builder and its overlays are destroyed.
    std::optional<utils::filelock::FileLock> projectMutex;
    std::optional<utils::filelock::FileLock> projectYamlMutex;
    // An existing or fully resolved lock; absent until fresh sources and dependencies are ready.
    std::optional<api::types::v1::BuilderProjectLock> projectLock;
    // Source changes waiting for dependency resolution, including an empty source list.
    std::optional<std::vector<api::types::v1::BuilderSourceLockEntry>> pendingSources;

    int64_t uid;
    int64_t gid;

    std::optional<package::Reference> projectRef;
    std::vector<std::string> packageModules;
    std::unique_ptr<utils::OverlayFS> baseOverlay;
    std::unique_ptr<utils::OverlayFS> runtimeOverlay;
    std::string installPrefix;
    runtime::RunContext buildContext;

    // capabilities for build stage
    static std::vector<std::string> privilegeBuilderCaps;
};

} // namespace linglong::builder
