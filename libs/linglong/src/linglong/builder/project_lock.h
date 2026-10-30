// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/BuilderProjectLock.hpp"
#include "linglong/api/types/v1/BuilderProjectSource.hpp"
#include "linglong/builder/source_fetcher.h"
#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace linglong::builder {

enum class ProjectLockMode { Generate, Update };

using LockedDependency = api::types::v1::BuilderProjectLockDependency;

struct ProjectSourcePlan
{
    std::vector<SourceFetchRequest> requests;
    bool changed{ false };
};

enum class SourceFetchStatus { Fetching, Complete, Failed };

using SourceFetchProgress =
  std::function<void(std::size_t, const api::types::v1::BuilderProjectSource &, SourceFetchStatus)>;

struct ResolveDependencyRequest
{
    std::string requested;
    std::string type;
};

struct LockedDependencyRequest
{
    LockedDependency entry;
};

using DependencyFetchRequest = std::variant<ResolveDependencyRequest, LockedDependencyRequest>;

struct ProjectDependencyPlan
{
    // An absent base request must be completed from the fetched runtime's package information.
    std::optional<DependencyFetchRequest> base;
    std::optional<DependencyFetchRequest> runtime;
    bool changed{ false };
};

auto projectLockPath(const std::filesystem::path &manifest) -> std::filesystem::path;

auto readProjectLock(const std::filesystem::path &path) noexcept
  -> utils::error::Result<api::types::v1::BuilderProjectLock>;

auto resolveProjectLockSources(
  const std::optional<std::vector<api::types::v1::BuilderProjectSource>> &sources,
  const std::optional<api::types::v1::BuilderProjectLock> &existingLock) noexcept
  -> utils::error::Result<ProjectSourcePlan>;

auto verifyProjectLockSources(
  const std::optional<std::vector<api::types::v1::BuilderProjectSource>> &sources,
  const api::types::v1::BuilderProjectLock &lock) noexcept
  -> utils::error::Result<ProjectSourcePlan>;

auto fetchProjectSources(const ProjectSourcePlan &plan,
                         const std::filesystem::path &cache,
                         const std::filesystem::path &destination,
                         bool offline,
                         const SourceFetchProgress &progress = {}) noexcept
  -> utils::error::Result<std::vector<api::types::v1::BuilderSourceLockEntry>>;

auto completeProjectLock(std::vector<api::types::v1::BuilderSourceLockEntry> sources,
                         LockedDependency base,
                         std::optional<LockedDependency> runtime = std::nullopt)
  -> api::types::v1::BuilderProjectLock;

auto writeCompleteProjectLock(const std::filesystem::path &manifest,
                              const api::types::v1::BuilderProjectLock &lock) noexcept
  -> utils::error::Result<void>;

auto lockedDependencyReference(const LockedDependency &entry) noexcept
  -> utils::error::Result<package::Reference>;

auto projectLockDependencyState(const api::types::v1::BuilderProjectLock &lock) noexcept
  -> utils::error::Result<std::string>;

auto lockedDependency(const api::types::v1::BuilderProjectLock &lock, const std::string &type)
  -> std::optional<LockedDependency>;

auto planProjectDependencies(const std::optional<std::string> &base,
                             const std::optional<std::string> &runtime,
                             const std::optional<api::types::v1::BuilderProjectLock> &existingLock,
                             bool locked) noexcept -> utils::error::Result<ProjectDependencyPlan>;

} // namespace linglong::builder
