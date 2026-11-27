// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "refresh.h"

#include "config.h"
#include "generate.h"
#include "linglong/utils/file.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/temporary_directory.h"
#include "linglong/utils/unique_fd.h"

#include <fmt/format.h>

#include <cerrno>
#include <chrono>
#include <map>
#include <set>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong::cdi_drv {

namespace {

namespace fs = std::filesystem;
using utils::error::Result;
using utils::filelock::LockType;
using SourceNames = std::set<std::string>;
using SourceSpecs = std::map<std::string, fs::path>;

constexpr auto fileMode =
  fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;
constexpr auto directoryMode =
  fileMode | fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;

Result<fs::file_status> entryStatus(const fs::path &path)
{
    LINGLONG_TRACE(fmt::format("check {}", path));
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return LINGLONG_ERR("check directory entry", ec);
    }
    return status;
}

Result<void> createDirectory(const fs::path &path)
{
    LINGLONG_TRACE(fmt::format("prepare CDI directory {}", path));

    std::error_code ec;
    if (fs::create_directory(path, ec)) {
        fs::permissions(path, directoryMode, ec);
    }
    if (ec) {
        return LINGLONG_ERR("create directory or set its permissions", ec);
    }
    return LINGLONG_OK;
}

Result<void> ensureRuntimeDirectory(const fs::path &path)
{
    LINGLONG_TRACE(fmt::format("prepare CDI runtime directory {}", path));

    auto directory = createDirectory(path);
    if (!directory) {
        return LINGLONG_ERR(directory);
    }

    struct stat status{};
    if (::lstat(path.c_str(), &status) < 0) {
        return LINGLONG_ERR("check runtime directory",
                            std::error_code(errno, std::generic_category()));
    }
    if (!S_ISDIR(status.st_mode) || status.st_uid != ::geteuid()
        || (status.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        return LINGLONG_ERR("expected a directory writable only by its owner");
    }
    return LINGLONG_OK;
}

Result<std::vector<fs::path>> regularFiles(const fs::path &directory)
{
    LINGLONG_TRACE(fmt::format("list files in {}", directory));

    std::error_code ec;
    fs::directory_iterator entry(directory, ec);
    const fs::directory_iterator end;
    if (ec == std::errc::no_such_file_or_directory) {
        return LINGLONG_OK;
    }

    std::vector<fs::path> files;
    for (; !ec && entry != end; entry.increment(ec)) {
        auto status = entryStatus(entry->path());
        if (!status) {
            return LINGLONG_ERR(status);
        }
        if (fs::is_regular_file(*status)) {
            files.push_back(entry->path().filename());
        }
    }
    if (ec) {
        return LINGLONG_ERR("iterate directory", ec);
    }
    return files;
}

Result<fs::path> currentDirectory(const fs::path &runtimeDir)
{
    LINGLONG_TRACE("locate current CDI generation");

    const auto pointer = runtimeDir / "current";
    auto status = entryStatus(pointer);
    if (!status) {
        return LINGLONG_ERR(status);
    }
    if (!fs::exists(*status)) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    const auto target = fs::read_symlink(pointer, ec);
    if (ec) {
        return LINGLONG_ERR("read current generation link", ec);
    }

    const auto name = target.string();
    if (target.has_parent_path() || name.compare(0, 11, "generation-") != 0) {
        return LINGLONG_ERR("invalid CDI current generation: " + name);
    }
    const auto path = runtimeDir / target;
    status = entryStatus(path);
    if (!status) {
        return LINGLONG_ERR(status);
    }
    if (!fs::is_directory(*status)) {
        return LINGLONG_ERR(fmt::format("current generation directory is missing: {}", path));
    }
    return path;
}

Result<void> ensureAlias(const fs::path &link, const fs::path &target)
{
    LINGLONG_TRACE(fmt::format("prepare CDI link {}", link));

    auto status = entryStatus(link);
    if (!status) {
        return LINGLONG_ERR(status);
    }
    std::error_code ec;
    if (!fs::exists(*status)) {
        fs::create_symlink(target, link, ec);
    } else if (fs::read_symlink(link, ec) != target && !ec) {
        return LINGLONG_ERR("unexpected link target");
    }
    if (ec) {
        return LINGLONG_ERR("create or read alias", ec);
    }
    return LINGLONG_OK;
}

Result<utils::filelock::FileLock> lockRequests(const fs::path &runtimeDir)
{
    LINGLONG_TRACE("lock CDI refresh request queue");

    auto lock = utils::filelock::FileLock::create(runtimeDir / "requests.lock", LockType::Write);
    if (!lock) {
        return LINGLONG_ERR("open CDI requests lock", lock);
    }
    auto acquired = lock->lock(LockType::Write);
    if (!acquired) {
        return LINGLONG_ERR("lock CDI requests", acquired);
    }
    return lock;
}

Result<RefreshRequests> claimRequests(const fs::path &runtimeDir)
{
    LINGLONG_TRACE("claim CDI refresh requests");
    // Keep queue changes separate from the lock held while generating configurations.
    auto lock = lockRequests(runtimeDir);
    if (!lock) {
        return LINGLONG_ERR(lock);
    }
    const auto pending = runtimeDir / "pending";
    const auto singles = pending / "refresh-single";
    const auto all = pending / "refresh-all";
    auto status = entryStatus(all);
    if (!status) {
        return LINGLONG_ERR(status);
    }

    // Remove markers before generation so later requests can create new markers.
    RefreshRequests requests;
    std::error_code ec;
    if (fs::is_regular_file(*status)) {
        requests.all = fs::remove(all, ec);
        if (ec) {
            return LINGLONG_ERR("claim refresh-all", ec);
        }
        if (requests.all) {
            fs::remove_all(singles, ec);
            if (ec) {
                return LINGLONG_ERR("clear single refresh requests", ec);
            }
            return requests;
        }
    }

    auto files = regularFiles(singles);
    if (!files) {
        return LINGLONG_ERR(files);
    }
    for (const auto &file : *files) {
        auto name = file.string();
        if (name.front() == '.') {
            continue;
        }
        if (fs::remove(singles / file, ec)) {
            if (validConfigName(name)) {
                requests.names.insert(std::move(name));
            } else {
                LogW("Skipping CDI refresh request {} because its configuration name is invalid",
                     name);
            }
        }
        if (ec) {
            return LINGLONG_ERR(fmt::format("claim request {}", file), ec);
        }
    }
    return requests;
}

using Artifacts = std::map<std::string, GeneratedFiles>;

Result<SourceSpecs> listSpecFiles(const fs::path &directory)
{
    LINGLONG_TRACE("list published CDI spec files");

    auto files = regularFiles(directory);
    if (!files) {
        return LINGLONG_ERR(files);
    }
    SourceSpecs specs;
    for (const auto &file : *files) {
        const auto extension = file.extension();
        const auto name = file.stem().string();
        if (file.native().front() != '.'
            && (extension == ".json" || extension == ".yaml" || extension == ".yml")) {
            specs.emplace(name, file);
        }
    }
    return specs;
}

struct GeneratedBatch
{
    Artifacts files;
    WatchFiles watches;
};

Result<GeneratedBatch> generateFiles(const fs::path &configDir, const RefreshRequests &requests)
{
    LINGLONG_TRACE("generate CDI source artifacts");

    auto names = configNames(configDir, requests);
    if (!names) {
        return LINGLONG_ERR(names);
    }
    GeneratedBatch batch;
    for (const auto &name : *names) {
        auto config = loadConfig(configDir / (name + ".json"));
        if (!config) {
            return LINGLONG_ERR(config);
        }
        if (!*config) {
            continue;
        }
        auto watch = generateWatch(name, **config);
        if (!watch) {
            LogE("Skipping CDI watches for {}: {}", name, watch.error().message());
        } else if (*watch) {
            batch.watches.emplace(name, std::move(**watch));
        }
        auto generated = generate(name, **config);
        if (!generated) {
            LogE("Skipping CDI source {}: {}", name, generated.error().message());
        } else if (*generated) {
            batch.files.emplace(name, std::move(**generated));
        }
    }
    return batch;
}

Result<bool> stageFile(const fs::path &destination,
                       const fs::path &previous,
                       const std::string *content)
{
    LINGLONG_TRACE(fmt::format("stage {}", destination));

    if (content != nullptr) {
        if (!previous.empty()) {
            auto oldContent = utils::readFile(previous);
            if (!oldContent) {
                return LINGLONG_ERR(oldContent);
            }
            if (*oldContent == *content) {
                content = nullptr;
            }
        }

        if (content != nullptr) {
            auto result = utils::writeFile(destination, *content, fileMode);
            if (!result) {
                return LINGLONG_ERR(result);
            }
            return true;
        }
    }

    std::error_code ec;
    fs::create_hard_link(previous, destination, ec);
    if (ec) {
        return LINGLONG_ERR("link unchanged artifact", ec);
    }
    return false;
}

Result<void> publish(const fs::path &runtimeDir,
                     const fs::path &current,
                     const SourceSpecs &previous,
                     const SourceNames &next,
                     const Artifacts &files)
{
    LINGLONG_TRACE("publish CDI generation");

    if (next.empty()) {
        if (!current.empty()) {
            std::error_code ec;
            // Detach both output directories by removing their shared pointer.
            fs::remove(runtimeDir / "current", ec);
            if (ec) {
                return LINGLONG_ERR("unpublish CDI generation", ec);
            }
        }
        return LINGLONG_OK;
    }

    for (const auto name : { "spec", "cfg" }) {
        auto result = ensureAlias(runtimeDir / name, fs::path("current") / name);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    auto staging = utils::TemporaryDirectory::create("generation-", runtimeDir);
    if (!staging) {
        return LINGLONG_ERR(staging);
    }
    const auto &destination = staging->path();
    for (const auto name : { "spec", "cfg" }) {
        auto result = createDirectory(destination / name);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    bool changed = current.empty() || previous.size() != next.size();
    for (const auto &name : next) {
        const auto cfg = fs::path("cfg") / (name + ".json");
        const auto artifact = files.find(name);
        const auto old = previous.find(name);
        const auto hadFiles = old != previous.end();
        const auto spec = fs::path("spec")
          / (artifact == files.end() ? old->second : fs::path(artifact->second.specFilename));
        const auto previousSpec = hadFiles ? fs::path("spec") / old->second : fs::path{};
        auto specResult = stageFile(destination / spec,
                                    hadFiles ? current / previousSpec : fs::path{},
                                    artifact == files.end() ? nullptr : &artifact->second.spec);
        if (!specResult) {
            return LINGLONG_ERR(specResult);
        }
        auto configResult = stageFile(destination / cfg,
                                      hadFiles ? current / cfg : fs::path{},
                                      artifact == files.end() ? nullptr : &artifact->second.config);
        if (!configResult) {
            return LINGLONG_ERR(configResult);
        }
        changed = changed || spec != previousSpec || *specResult || *configResult;
    }

    if (!changed) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    fs::permissions(destination, directoryMode, ec);
    if (ec) {
        return LINGLONG_ERR("set generation directory permissions", ec);
    }
    // Stage the pointer on the same filesystem before switching both output directories.
    const auto link = destination / "current.next";
    fs::create_symlink(destination.filename(), link, ec);
    if (ec) {
        return LINGLONG_ERR("stage current generation link", ec);
    }
    fs::rename(link, runtimeDir / "current", ec);
    if (ec) {
        return LINGLONG_ERR("switch current generation", ec);
    }
    const auto published = staging->keep();
    LogI("Published CDI generation {}", published.filename());
    return LINGLONG_OK;
}

Result<void> refreshGeneration(const fs::path &configDir,
                               const fs::path &runtimeDir,
                               const RefreshRequests &requests,
                               const WatchOptions &watches)
{
    LINGLONG_TRACE("refresh CDI generation");

    auto current = currentDirectory(runtimeDir);
    if (!current) {
        return LINGLONG_ERR(current);
    }
    auto previous = current->empty() ? Result<SourceSpecs>{} : listSpecFiles(*current / "spec");
    if (!previous) {
        return LINGLONG_ERR(previous);
    }
    auto batch = generateFiles(configDir, requests);
    if (!batch) {
        return LINGLONG_ERR(batch);
    }
    auto synced = syncWatches(batch->watches, requests, watches);
    if (!synced) {
        LogE("Failed to synchronize CDI watches: {}", synced.error().message());
    }

    SourceNames next;
    if (!requests.all) {
        for (const auto &[name, spec] : *previous) {
            next.insert(name);
        }
        for (const auto &name : requests.names) {
            next.erase(name);
        }
    }
    for (const auto &entry : batch->files) {
        next.emplace(entry.first);
    }
    return publish(runtimeDir, *current, *previous, next, batch->files);
}

Result<bool> refreshBatch(const RefreshOptions &options)
{
    LINGLONG_TRACE("refresh CDI request batch");

    auto lock =
      utils::filelock::FileLock::create(options.runtimeDir / "refresh.lock", LockType::Write);
    if (!lock) {
        return LINGLONG_ERR("open CDI refresh lock", lock);
    }
    auto acquired = lock->lock(LockType::Write);
    if (!acquired) {
        return LINGLONG_ERR("lock CDI refresh", acquired);
    }

    RefreshRequests requests;
    if (options.pending) {
        auto pending = claimRequests(options.runtimeDir);
        if (!pending) {
            return LINGLONG_ERR(pending);
        }
        requests = std::move(*pending);
    } else if (options.name.empty()) {
        requests.all = true;
    } else {
        requests.names.insert(options.name);
    }
    if (!requests.all && requests.names.empty()) {
        return false;
    }
    // Claim the batch first: requests arriving during this refresh belong to the next batch.
    auto result =
      refreshGeneration(options.configDir, options.runtimeDir, requests, options.watches);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    return true;
}

} // namespace

Result<void> requestRefresh(const fs::path &runtimeDir, const std::string &name)
{
    LINGLONG_TRACE("queue CDI refresh");

    if (!name.empty() && !validConfigName(name)) {
        return LINGLONG_ERR("invalid CDI configuration name");
    }

    auto directory = ensureRuntimeDirectory(runtimeDir);
    if (!directory) {
        return LINGLONG_ERR(directory);
    }

    auto lock = lockRequests(runtimeDir);
    if (!lock) {
        return LINGLONG_ERR(lock);
    }
    const auto pending = runtimeDir / "pending";
    directory = createDirectory(pending);
    if (!directory) {
        return LINGLONG_ERR(directory);
    }
    auto marker = pending / "refresh-all";
    if (!name.empty()) {
        const auto singles = pending / "refresh-single";
        directory = createDirectory(singles);
        if (!directory) {
            return LINGLONG_ERR(directory);
        }
        marker = singles / name;
    }
    const utils::fd::UniqueFd fd{
        ::open(marker.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644)
    };
    if (fd) {
        return LINGLONG_OK;
    }

    const std::error_code ec(errno, std::generic_category());
    if (ec == std::errc::file_exists) {
        auto status = entryStatus(marker);
        if (!status) {
            return LINGLONG_ERR(status);
        }
        if (fs::is_regular_file(*status)) {
            return LINGLONG_OK;
        }
    }
    return LINGLONG_ERR(fmt::format("create refresh request {}", marker), ec);
}

utils::error::Result<void> refresh(const RefreshOptions &options)
{
    LINGLONG_TRACE("refresh CDI driver configurations");

    if (!options.name.empty() && (!validConfigName(options.name) || options.pending)) {
        return LINGLONG_ERR("invalid CDI refresh selection");
    }

    auto directory = ensureRuntimeDirectory(options.runtimeDir);
    if (!directory) {
        return LINGLONG_ERR(directory);
    }

    constexpr auto pollInterval = std::chrono::milliseconds(200);
    auto idleSince = std::chrono::steady_clock::now();
    for (;;) {
        auto processed = refreshBatch(options);
        if (!processed) {
            return LINGLONG_ERR(processed);
        }
        if (!options.pending) {
            return LINGLONG_OK;
        }
        if (*processed) {
            idleSince = std::chrono::steady_clock::now();
            continue;
        }
        if (std::chrono::steady_clock::now() - idleSince >= options.idleTimeout) {
            return LINGLONG_OK;
        }
        // refreshBatch releases the refresh lock before waiting for more requests.
        std::this_thread::sleep_for(pollInterval);
    }
}

} // namespace linglong::cdi_drv
