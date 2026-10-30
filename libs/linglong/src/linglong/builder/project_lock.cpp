// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "project_lock.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/source_cache.h"
#include "linglong/builder/source_fetcher.h"
#include "linglong/package/architecture.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/yaml.h"
#include "ytj/ytj.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include <unistd.h>

namespace linglong::builder {
namespace {

using api::types::v1::BuilderProjectLock;
using api::types::v1::BuilderProjectSource;
using api::types::v1::BuilderSourceLockEntry;
using api::types::v1::BuilderSourceLockResolved;

BuilderProjectSource normalizedSource(const BuilderProjectSource &source)
{
    auto normalized = source;
    if (normalized.digest) {
        if (normalized.digest->rfind("sha256:", 0) != 0) {
            *normalized.digest = "sha256:" + *normalized.digest;
        }
        std::transform(normalized.digest->begin(),
                       normalized.digest->end(),
                       normalized.digest->begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
    }

    if (source.kind == "git") {
        normalized.submodules = source.submodules.value_or(true);
    } else {
        normalized.commit.reset();
        normalized.submodules.reset();
    }

    return normalized;
}

bool sourceFetchEqual(const BuilderProjectSource &left, const BuilderProjectSource &right)
{
    // Output names and version descriptions do not affect the fetched content.
    return left.commit == right.commit && left.digest == right.digest && left.kind == right.kind
      && left.submodules == right.submodules && left.url == right.url;
}

auto matchLockEntries(const std::vector<BuilderProjectSource> &sources,
                      const BuilderProjectLock &lock)
  -> utils::error::Result<std::vector<BuilderSourceLockResolved>>
{
    LINGLONG_TRACE("match project sources with project lock");

    if (lock.sources.size() != sources.size()) {
        return LINGLONG_ERR("sources differ from linglong.lock; run ll-builder update-lockfile");
    }

    std::vector<bool> used(lock.sources.size(), false);
    std::vector<BuilderSourceLockResolved> resolvedSources;
    resolvedSources.reserve(sources.size());
    for (const auto &source : sources) {
        const auto normalized = normalizedSource(source);
        bool found = false;
        for (std::size_t index = 0; index < lock.sources.size(); ++index) {
            const auto &entry = lock.sources[index];
            if (used[index] || !sourceFetchEqual(normalizedSource(entry.source), normalized)) {
                continue;
            }
            if (source.kind == "git" && !gitCommitFromHash(entry.resolved.hash)) {
                return LINGLONG_ERR("git source has an invalid hash in linglong.lock; run "
                                    "ll-builder update-lockfile");
            }

            used[index] = true;
            resolvedSources.push_back(entry.resolved);
            found = true;
            break;
        }

        if (!found) {
            return LINGLONG_ERR(
              "sources differ from linglong.lock; run ll-builder update-lockfile");
        }
    }

    return resolvedSources;
}

bool validDependencies(const BuilderProjectLock &lock)
{
    std::set<std::string> types;
    for (const auto &entry : lock.dependencies) {
        const auto reference = lockedDependencyReference(entry);
        if ((entry.type != "base" && entry.type != "runtime") || !types.insert(entry.type).second
            || !reference || entry.resolved.modules.empty()) {
            return false;
        }

        std::set<std::string> modules;
        for (const auto &module : entry.resolved.modules) {
            if (module.empty() || !modules.insert(module).second) {
                return false;
            }
        }

        if (modules.count("binary") == 0 && modules.count("runtime") == 0) {
            return false;
        }
    }

    return types.count("base") == 1;
}

utils::error::Result<std::string> validateSource(const api::types::v1::BuilderProjectSource &source)
{
    LINGLONG_TRACE("validate locked source");

    if (source.kind != "git" && source.kind != "file" && source.kind != "archive"
        && source.kind != "dsc") {
        return LINGLONG_ERR("unsupported source kind: " + source.kind);
    }
    if (!source.url) {
        return LINGLONG_ERR("source URL is missing");
    }
    if (source.kind == "git" && !source.commit) {
        return LINGLONG_ERR("git source commit is missing");
    }
    if (source.kind == "git" && source.digest) {
        return LINGLONG_ERR("git source does not support digest");
    }

    if (source.digest) {
        auto value = *source.digest;
        if (value.rfind("sha256:", 0) == 0) {
            value.erase(0, 7);
        }
        if (value.size() != 64
            || !std::all_of(value.begin(), value.end(), [](unsigned char character) {
                   return std::isxdigit(character) != 0;
               })) {
            return LINGLONG_ERR("source digest must be a SHA-256 value");
        }
    }

    const auto name = sourceOutputName(source);
    if (name.empty() || name == "." || name == ".." || name == ".git"
        || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        return LINGLONG_ERR("source name must be a single path component");
    }

    return name;
}

auto validateSources(const std::vector<BuilderProjectSource> &sources) -> utils::error::Result<void>
{
    LINGLONG_TRACE("validate project sources");

    std::set<std::string> outputNames;
    std::vector<BuilderProjectSource> requests;
    requests.reserve(sources.size());
    for (const auto &source : sources) {
        auto name = validateSource(source);
        if (!name) {
            return LINGLONG_ERR(name);
        }
        if (!outputNames.insert(*name).second) {
            return LINGLONG_ERR("multiple sources use the same output name: " + *name);
        }

        auto normalized = normalizedSource(source);
        const auto duplicate = std::find_if(requests.begin(), requests.end(), [&](const auto &old) {
            return sourceFetchEqual(old, normalized);
        });
        if (duplicate != requests.end()) {
            return LINGLONG_ERR("duplicate source fetch request: " + sourceOutputName(*duplicate)
                                + " and " + *name);
        }

        requests.push_back(std::move(normalized));
    }

    return LINGLONG_OK;
}

auto ensureSourceDestination(SourceCacheLayout &cache, const std::filesystem::path &destination)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("ensure source destination");

    auto ensured = cache.ensure();
    if (!ensured) {
        return LINGLONG_ERR(ensured);
    }

    std::error_code destinationError;
    std::filesystem::create_directories(destination, destinationError);
    if (destinationError) {
        return LINGLONG_ERR("failed to create source destination: " + destinationError.message());
    }

    return LINGLONG_OK;
}

} // namespace

auto projectLockPath(const std::filesystem::path &manifest) -> std::filesystem::path
{
    auto result = manifest;
    result.replace_extension(".lock");
    return result;
}

auto lockedDependency(const BuilderProjectLock &lock, const std::string &type)
  -> std::optional<LockedDependency>
{
    for (const auto &entry : lock.dependencies) {
        if (entry.type == type) {
            return entry;
        }
    }
    return std::nullopt;
}

auto readProjectLock(const std::filesystem::path &path) noexcept
  -> utils::error::Result<BuilderProjectLock>
{
    LINGLONG_TRACE("read project lock");

    auto loaded = utils::serialize::LoadYAMLFile<BuilderProjectLock>(path);
    if (!loaded) {
        return LINGLONG_ERR("failed to read " + path.string() + "; run ll-builder update-lockfile",
                            loaded);
    }

    if (loaded->version != 1) {
        return LINGLONG_ERR("unsupported project lock version; run ll-builder update-lockfile: "
                            + path.string());
    }

    if (!validDependencies(*loaded)) {
        return LINGLONG_ERR(
          "project lock dependencies are incomplete; run ll-builder update-lockfile");
    }

    return std::move(*loaded);
}

auto completeProjectLock(std::vector<BuilderSourceLockEntry> sources,
                         LockedDependency base,
                         std::optional<LockedDependency> runtime) -> BuilderProjectLock
{
    BuilderProjectLock lock;
    lock.version = 1;
    lock.sources = std::move(sources);
    lock.dependencies = { std::move(base) };
    if (runtime) {
        lock.dependencies.push_back(std::move(*runtime));
    }
    return lock;
}

auto writeCompleteProjectLock(const std::filesystem::path &manifest,
                              const BuilderProjectLock &lock) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("write complete project lock");

    try {
        if (!validDependencies(lock)) {
            return LINGLONG_ERR("project lock dependencies are incomplete");
        }

        const auto output = ytj::to_yaml(lock);
        const auto path = projectLockPath(manifest);
        auto temporary = path;
        temporary += ".tmp." + std::to_string(::getpid());

        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) {
                return LINGLONG_ERR("failed to create " + temporary.string());
            }
            file << output << '\n';
            file.flush();
            if (!file) {
                return LINGLONG_ERR("failed to write " + temporary.string());
            }
        }

        std::error_code ec;
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::filesystem::remove(temporary);
            return LINGLONG_ERR("failed to replace " + path.string() + ": " + ec.message());
        }
        return LINGLONG_OK;
    } catch (...) {
        return LINGLONG_ERR(std::current_exception());
    }
}

auto lockedDependencyReference(const LockedDependency &entry) noexcept
  -> utils::error::Result<package::Reference>
{
    LINGLONG_TRACE("resolve locked dependency architecture");

    auto locked = package::FuzzyReference::parse(entry.resolved.reference);
    if (!locked) {
        return LINGLONG_ERR(locked);
    }
    if (!locked->channel || !locked->version) {
        return LINGLONG_ERR("locked dependency must contain channel, ID and exact version; "
                            "run ll-builder update-lockfile");
    }
    auto version = package::Version::parse(*locked->version);
    if (!version) {
        return LINGLONG_ERR(version);
    }

    return package::Reference::create(
      *locked->channel,
      locked->id,
      *version,
      locked->arch.value_or(package::Architecture::currentCPUArchitecture()));
}

auto projectLockDependencyState(const BuilderProjectLock &lock) noexcept
  -> utils::error::Result<std::string>
{
    LINGLONG_TRACE("get project lock dependency state");

    try {
        if (!validDependencies(lock)) {
            return LINGLONG_ERR("project lock dependencies are incomplete");
        }

        auto state = nlohmann::json::object();
        for (const auto &entry : lock.dependencies) {
            auto reference = lockedDependencyReference(entry);
            if (!reference) {
                return LINGLONG_ERR(reference);
            }

            auto resolved = entry.resolved;
            resolved.reference = reference->toString();
            std::sort(resolved.modules.begin(), resolved.modules.end());
            state[entry.type] = std::move(resolved);
        }

        return state.dump();
    } catch (...) {
        return LINGLONG_ERR(std::current_exception());
    }
}

namespace {

auto planDependency(const std::optional<std::string> &requested,
                    const std::optional<LockedDependency> &entry,
                    const std::string &type,
                    bool locked) -> utils::error::Result<std::optional<DependencyFetchRequest>>
{
    LINGLONG_TRACE("check locked dependency declaration");

    if (!requested && !entry) {
        return std::optional<DependencyFetchRequest>{};
    }
    if (requested && entry && entry->requested == *requested) {
        return std::optional<DependencyFetchRequest>{ LockedDependencyRequest{ *entry } };
    }

    if (locked) {
        return LINGLONG_ERR(type + " differs from linglong.lock; run ll-builder update-lockfile");
    }
    if (!requested) {
        return std::optional<DependencyFetchRequest>{};
    }
    return std::optional<DependencyFetchRequest>{ ResolveDependencyRequest{ *requested, type } };
}

} // namespace

auto planProjectDependencies(const std::optional<std::string> &base,
                             const std::optional<std::string> &runtime,
                             const std::optional<api::types::v1::BuilderProjectLock> &existingLock,
                             bool locked) noexcept -> utils::error::Result<ProjectDependencyPlan>
{
    LINGLONG_TRACE("plan project dependencies");

    if (!base && !runtime) {
        return LINGLONG_ERR("at least one of base or runtime must be specified");
    }

    if (existingLock && !validDependencies(*existingLock)) {
        return LINGLONG_ERR("project lock dependencies are incomplete or inconsistent; run "
                            "ll-builder update-lockfile");
    }

    ProjectDependencyPlan plan;
    plan.changed = !existingLock;

    const auto oldRuntime =
      existingLock ? lockedDependency(*existingLock, "runtime") : std::nullopt;
    auto runtimeRequest = planDependency(runtime, oldRuntime, "runtime", locked);
    if (!runtimeRequest) {
        return LINGLONG_ERR(runtimeRequest);
    }
    plan.runtime = std::move(*runtimeRequest);
    plan.changed |= plan.runtime ? std::holds_alternative<ResolveDependencyRequest>(*plan.runtime)
                                 : oldRuntime.has_value();

    if (!base) {
        return plan;
    }

    const auto oldBase = existingLock ? lockedDependency(*existingLock, "base") : std::nullopt;
    auto baseRequest = planDependency(base, oldBase, "base", locked);
    if (!baseRequest) {
        return LINGLONG_ERR(baseRequest);
    }
    plan.base = std::move(*baseRequest);
    plan.changed |= std::holds_alternative<ResolveDependencyRequest>(*plan.base);

    return plan;
}

auto resolveProjectLockSources(const std::optional<std::vector<BuilderProjectSource>> &sources,
                               const std::optional<BuilderProjectLock> &existingLock) noexcept
  -> utils::error::Result<ProjectSourcePlan>
{
    LINGLONG_TRACE("plan project lock sources");

    ProjectSourcePlan plan;
    if (!sources || sources->empty()) {
        plan.changed = !existingLock || !existingLock->sources.empty();
        return plan;
    }

    auto validated = validateSources(*sources);
    if (!validated) {
        return LINGLONG_ERR(validated);
    }

    plan.changed = !existingLock;
    plan.requests.reserve(sources->size());
    std::vector<bool> used(existingLock ? existingLock->sources.size() : 0, false);
    for (const auto &source : *sources) {
        const BuilderSourceLockEntry *matched = nullptr;
        if (existingLock) {
            const auto normalized = normalizedSource(source);
            for (std::size_t old = 0; old < existingLock->sources.size(); ++old) {
                if (used[old]
                    || !sourceFetchEqual(normalizedSource(existingLock->sources[old].source),
                                         normalized)) {
                    continue;
                }
                if (source.kind == "git"
                    && !gitCommitFromHash(existingLock->sources[old].resolved.hash)) {
                    continue;
                }
                used[old] = true;
                matched = &existingLock->sources[old];
                break;
            }
        }

        if (matched) {
            plan.requests.emplace_back(LockedSourceRequest{ source, matched->resolved });
        } else {
            plan.requests.emplace_back(GenerateSourceRequest{ source });
            plan.changed = true;
        }
    }

    if (std::any_of(used.begin(), used.end(), [](bool matched) {
            return !matched;
        })) {
        plan.changed = true;
    }

    return plan;
}

auto verifyProjectLockSources(const std::optional<std::vector<BuilderProjectSource>> &sources,
                              const BuilderProjectLock &lock) noexcept
  -> utils::error::Result<ProjectSourcePlan>
{
    LINGLONG_TRACE("verify project lock sources");

    if (!sources || sources->empty()) {
        if (!lock.sources.empty()) {
            return LINGLONG_ERR(
              "sources differ from linglong.lock; run ll-builder update-lockfile");
        }
        return ProjectSourcePlan{};
    }

    auto validated = validateSources(*sources);
    if (!validated) {
        return LINGLONG_ERR(validated);
    }

    auto matched = matchLockEntries(*sources, lock);
    if (!matched) {
        return LINGLONG_ERR(matched);
    }

    ProjectSourcePlan plan;
    plan.requests.reserve(sources->size());
    for (std::size_t index = 0; index < sources->size(); ++index) {
        plan.requests.emplace_back(LockedSourceRequest{ (*sources)[index], (*matched)[index] });
    }

    return plan;
}

auto fetchProjectSources(const ProjectSourcePlan &plan,
                         const std::filesystem::path &cache,
                         const std::filesystem::path &destination,
                         bool offline,
                         const SourceFetchProgress &progress) noexcept
  -> utils::error::Result<std::vector<BuilderSourceLockEntry>>
{
    LINGLONG_TRACE("fetch project sources");

    SourceCacheLayout cacheLayout(cache);
    auto preparedDestination = ensureSourceDestination(cacheLayout, destination);
    if (!preparedDestination) {
        return LINGLONG_ERR(preparedDestination);
    }

    std::vector<BuilderSourceLockEntry> entries;
    entries.reserve(plan.requests.size());
    for (std::size_t index = 0; index < plan.requests.size(); ++index) {
        const auto &request = plan.requests[index];
        const auto &source = std::visit(
          [](const auto &request) -> const BuilderProjectSource & {
              return request.source;
          },
          request);
        const auto report = [&](SourceFetchStatus status) {
            if (progress) {
                progress(index, source, status);
            }
        };

        report(SourceFetchStatus::Fetching);
        SourceFetcher fetcher(request, cacheLayout);
        auto fetched = fetcher.fetch(destination, offline);
        if (!fetched) {
            report(SourceFetchStatus::Failed);
            return LINGLONG_ERR(fetched);
        }

        entries.push_back(BuilderSourceLockEntry{ std::move(*fetched), normalizedSource(source) });
        report(SourceFetchStatus::Complete);
    }

    return entries;
}

} // namespace linglong::builder
