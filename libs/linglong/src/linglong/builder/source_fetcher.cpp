/*
 * SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "source_fetcher.h"

#include "builder_script.h"
#include "linglong/common/formatter.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/sha256.h"
#include "linglong/utils/temporary_directory.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace linglong::builder {

namespace {

using api::types::v1::BuilderSourceLockResolved;

std::string finishHash(digest::SHA256 &hash)
{
    std::array<std::byte, 32> value{};
    hash.final(value.data());

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (auto byte : value) {
        output << std::setw(2) << std::to_integer<unsigned int>(byte);
    }

    return output.str();
}

int fromHex(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string percentDecode(std::string_view value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const auto high = fromHex(value[index + 1]);
            const auto low = fromHex(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }

        decoded.push_back(value[index]);
    }

    return decoded;
}

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char character) {
        return !std::isspace(character);
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lastOutputLine(const std::string &output)
{
    auto trimmed = trim(output);
    const auto separator = trimmed.rfind('\n');
    if (separator != std::string::npos) {
        trimmed.erase(0, separator + 1);
    }
    return trim(std::move(trimmed));
}

auto normalizeSha256(std::optional<std::string> hash) -> std::optional<std::string>
{
    if (hash && hash->rfind("sha256:", 0) != 0) {
        *hash = "sha256:" + *hash;
    }
    if (hash) {
        std::transform(hash->begin(), hash->end(), hash->begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    }
    return hash;
}

bool isSha256Hash(const std::string &hash, std::string_view prefix)
{
    if (hash.rfind(prefix, 0) != 0 || hash.size() != prefix.size() + 64) {
        return false;
    }

    return std::all_of(hash.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                       hash.end(),
                       [](unsigned char character) {
                           return std::isxdigit(character) != 0;
                       });
}

utils::error::Result<void> copyTree(const std::filesystem::path &source,
                                    const std::filesystem::path &destination) noexcept
{
    LINGLONG_TRACE("copy source tree");

    std::error_code ec;
    std::filesystem::remove_all(destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to remove source tree destination: " + ec.message());
    }

    auto copied = utils::copyDirectory(source, destination);
    if (!copied) {
        return LINGLONG_ERR(copied);
    }

    return LINGLONG_OK;
}

auto lockCacheEntry(const SourceCacheLayout &cache, const std::filesystem::path &entry) noexcept
  -> utils::error::Result<utils::filelock::FileLock>
{
    LINGLONG_TRACE("lock source cache entry");

    const auto path = cache.lockFile(entry);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create source cache lock directory: " + ec.message());
    }

    auto lock = utils::filelock::FileLock::create(path, utils::filelock::LockType::Write);
    if (!lock) {
        return LINGLONG_ERR(lock);
    }
    auto locked = lock->lock(utils::filelock::LockType::Write);
    if (!locked) {
        return LINGLONG_ERR(locked);
    }

    return lock;
}

template <typename Validator>
auto publishCacheEntry(const std::filesystem::path &staging,
                       const std::filesystem::path &destination,
                       Validator valid) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("publish source cache entry");

    // The caller holds the entry lock through publication and materialization.
    if (valid(destination)) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create source cache directory: " + ec.message());
    }
    std::filesystem::remove_all(destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to remove invalid source cache entry: " + ec.message());
    }

    std::filesystem::rename(staging, destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to publish source cache entry: " + ec.message());
    }
    return LINGLONG_OK;
}

auto replaceStagedDirectory(const std::filesystem::path &staging,
                            const std::filesystem::path &destination) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("replace materialized source directory");

    std::error_code ec;
    std::filesystem::remove_all(destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to remove old source destination: " + ec.message());
    }

    std::filesystem::rename(staging, destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to publish source destination: " + ec.message());
    }

    return LINGLONG_OK;
}

auto materializeFile(const std::filesystem::path &cached,
                     const std::filesystem::path &destination) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("materialize file source");

    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create source destination: " + ec.message());
    }

    auto temporary = utils::TemporaryDirectory::create(".materialize-", destination.parent_path());
    if (!temporary) {
        return LINGLONG_ERR(temporary);
    }

    const auto staging = temporary->path() / "source";
    std::filesystem::copy_file(cached,
                               staging,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        return LINGLONG_ERR("failed to copy cached source: " + ec.message());
    }

    std::filesystem::rename(staging, destination, ec);
    if (ec) {
        return LINGLONG_ERR("failed to publish source destination: " + ec.message());
    }

    return LINGLONG_OK;
}

} // namespace

auto fileNameFromUrl(std::string_view url) -> std::string
{
    const auto end = url.find_first_of("?#");
    const auto path = url.substr(0, end);
    const auto separator = path.rfind('/');
    return percentDecode(path.substr(separator == std::string_view::npos ? 0 : separator + 1));
}

auto sourceOutputName(const api::types::v1::BuilderProjectSource &source) -> std::string
{
    if (source.name) {
        return *source.name;
    }
    if (!source.url) {
        return {};
    }

    return fileNameFromUrl(*source.url);
}

namespace {

auto hashFileContents(digest::SHA256 &hash, const std::filesystem::path &path) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("hash directory source file contents");

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return LINGLONG_ERR("failed to open " + path.string());
    }

    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        hash.update(reinterpret_cast<const std::byte *>(buffer.data()), input.gcount());
    }

    if (!input.eof()) {
        return LINGLONG_ERR("failed to read " + path.string());
    }

    return LINGLONG_OK;
}

} // namespace

auto fileHash(const std::filesystem::path &file) noexcept -> utils::error::Result<std::string>
{
    LINGLONG_TRACE("calculate file hash");

    digest::SHA256 hash;
    auto result = hashFileContents(hash, file);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return "sha256:" + finishHash(hash);
}

namespace {

auto runFetchScript(
  const std::string &scriptName,
  std::vector<std::string> arguments,
  const std::vector<std::pair<std::string, std::string>> &environment = {}) noexcept
  -> utils::error::Result<std::string>
{
    LINGLONG_TRACE("run source fetch script");

    auto scriptFile = findBuilderScript(scriptName);
    if (!scriptFile) {
        return LINGLONG_ERR(scriptFile);
    }

    LogD("source fetch: running helper {} action={}", scriptName, arguments.front());
    arguments.insert(arguments.begin(), scriptFile->string());
    utils::Cmd command("sh");
    for (const auto &[name, value] : environment) {
        command.setEnv(name, value);
    }

    return command.exec(arguments);
}

struct SourceFetchContext
{
    const SourceFetchRequest &request;
    const SourceCacheLayout &cache;
    const std::filesystem::path &destination;
    bool offline;
};

auto requestSource(const SourceFetchRequest &request)
  -> const api::types::v1::BuilderProjectSource &
{
    return std::visit(
      [](const auto &value) -> const api::types::v1::BuilderProjectSource & {
          return value.source;
      },
      request);
}

auto requestExpectedHash(const SourceFetchRequest &request) -> std::optional<std::string>
{
    if (const auto *locked = std::get_if<LockedSourceRequest>(&request)) {
        return locked->resolved.hash;
    }

    return std::get<GenerateSourceRequest>(request).source.digest;
}

struct AcquiredSource
{
    std::filesystem::path artifact;
    BuilderSourceLockResolved result;
};

bool isGitCommitId(std::string_view commit)
{
    return (commit.size() == 40 || commit.size() == 64)
      && std::all_of(commit.begin(), commit.end(), [](unsigned char character) {
               return std::isxdigit(character) != 0;
           });
}

bool sameGitCommit(std::string_view left, std::string_view right)
{
    return left.size() == right.size()
      && std::equal(left.begin(),
                    left.end(),
                    right.begin(),
                    [](unsigned char lhs, unsigned char rhs) {
                        return std::tolower(lhs) == std::tolower(rhs);
                    });
}

class SourceFetchHandler
{
public:
    virtual ~SourceFetchHandler() = default;

    auto fetch(const SourceFetchContext &context) const noexcept
      -> utils::error::Result<BuilderSourceLockResolved>
    {
        LINGLONG_TRACE("fetch source with handler");

        const auto &source = requestSource(context.request);
        const auto name = sourceOutputName(source);
        const auto *lockedRequest = std::get_if<LockedSourceRequest>(&context.request);
        auto expected = normalizeExpectedHash(requestExpectedHash(context.request));
        if (!expected) {
            return LINGLONG_ERR(expected);
        }

        BuilderSourceLockResolved resolved;
        std::filesystem::path cached;
        std::optional<utils::filelock::FileLock> cacheLock;
        bool cacheReady = false;
        if (lockedRequest) {
            assert(expected->has_value());
            resolved.hash = **expected;
            cached = cachePath(context, resolved);
            auto locked = lockCacheEntry(context.cache, cached);
            if (!locked) {
                return LINGLONG_ERR(locked);
            }
            cacheLock.emplace(std::move(*locked));

            LogD("source fetch: validating cached source {} at {}", name, cached);
            cacheReady = validate(context, cached, resolved);
            if (cacheReady) {
                LogD("source fetch: verified cache hit for {}", name);
            } else {
                cacheLock.reset();
            }
        }

        if (!cacheReady) {
            if (context.offline) {
                return LINGLONG_ERR("cannot acquire a new or changed source in offline mode");
            }

            LogD("source fetch: acquiring {} ({})", name, source.kind);
            auto temporary = utils::TemporaryDirectory::create("fetch-", context.cache.temporary());
            if (!temporary) {
                return LINGLONG_ERR(temporary);
            }
            auto acquired = acquire(context, temporary->path());
            if (!acquired) {
                return LINGLONG_ERR(acquired);
            }

            if (*expected && **expected != acquired->result.hash) {
                LogD("source fetch: hash mismatch for {} expected={} actual={}",
                     name,
                     **expected,
                     acquired->result.hash);
                return LINGLONG_ERR(lockedRequest
                                      ? "source hash does not match linglong.lock"
                                      : "source hash does not match digest in linglong.yaml");
            }
            resolved = std::move(acquired->result);
            cached = cachePath(context, resolved);
            auto locked = lockCacheEntry(context.cache, cached);
            if (!locked) {
                return LINGLONG_ERR(locked);
            }
            cacheLock.emplace(std::move(*locked));

            LogD("source fetch: publishing {} to cache {}", name, cached);
            auto published = publishCacheEntry(acquired->artifact, cached, [&](const auto &path) {
                return validate(context, path, resolved);
            });
            if (!published) {
                return LINGLONG_ERR(published);
            }
        }

        LogD("source fetch: materializing {} from cache to {}", name, context.destination);
        auto materialized = materialize(context, cached, resolved);
        if (!materialized) {
            return LINGLONG_ERR(materialized);
        }

        return resolved;
    }

protected:
    virtual auto acquire(const SourceFetchContext &context,
                         const std::filesystem::path &temporary) const noexcept
      -> utils::error::Result<AcquiredSource> = 0;
    virtual bool validate(const SourceFetchContext &context,
                          const std::filesystem::path &cached,
                          const BuilderSourceLockResolved &resolved) const noexcept = 0;
    virtual auto materialize(const SourceFetchContext &context,
                             const std::filesystem::path &cached,
                             const BuilderSourceLockResolved &resolved) const noexcept
      -> utils::error::Result<void> = 0;
    virtual auto normalizeExpectedHash(std::optional<std::string> expected) const noexcept
      -> utils::error::Result<std::optional<std::string>> = 0;

    virtual auto cachePath(const SourceFetchContext &context,
                           const BuilderSourceLockResolved &resolved) const -> std::filesystem::path
    {
        return context.cache.object(requestSource(context.request).kind, resolved.hash);
    }
};

class BlobSourceHandler final : public SourceFetchHandler
{
public:
    enum class Materialization {
        File,
        Archive,
    };

    explicit BlobSourceHandler(Materialization materialization)
        : materialization(materialization)
    {
    }

private:
    auto normalizeExpectedHash(std::optional<std::string> expected) const noexcept
      -> utils::error::Result<std::optional<std::string>> override
    {
        LINGLONG_TRACE("validate expected file source hash");

        expected = normalizeSha256(std::move(expected));
        if (expected && !isSha256Hash(*expected, "sha256:")) {
            return LINGLONG_ERR("invalid source SHA-256 digest");
        }
        return expected;
    }

    auto acquire(const SourceFetchContext &context,
                 const std::filesystem::path &temporary) const noexcept
      -> utils::error::Result<AcquiredSource> override
    {
        LINGLONG_TRACE("acquire blob source");

        const auto &source = requestSource(context.request);
        const auto artifact = temporary / "source";
        auto downloaded = runFetchScript("fetch-" + source.kind + "-source",
                                         { "download", *source.url, artifact.string() });
        if (!downloaded) {
            return LINGLONG_ERR(downloaded);
        }

        auto hash = fileHash(artifact);
        if (!hash) {
            return LINGLONG_ERR(hash);
        }

        return AcquiredSource{ .artifact = artifact,
                               .result = BuilderSourceLockResolved{ .hash = std::move(*hash) } };
    }

    bool validate(const SourceFetchContext &,
                  const std::filesystem::path &cached,
                  const BuilderSourceLockResolved &resolved) const noexcept override
    {
        auto actual = fileHash(cached);
        return actual && *actual == resolved.hash;
    }

    auto materialize(const SourceFetchContext &context,
                     const std::filesystem::path &cached,
                     const BuilderSourceLockResolved &) const noexcept
      -> utils::error::Result<void> override
    {
        LINGLONG_TRACE("materialize blob source");

        const auto output = context.destination / sourceOutputName(requestSource(context.request));
        if (materialization == Materialization::File) {
            return materializeFile(cached, output);
        }

        auto temporary = utils::TemporaryDirectory::create(".materialize-", context.destination);
        if (!temporary) {
            return LINGLONG_ERR(temporary);
        }

        const auto staging = temporary->path() / "source";
        auto extracted = runFetchScript("fetch-archive-source",
                                        { "materialize", cached.string(), staging.string() });
        if (!extracted) {
            return LINGLONG_ERR(extracted);
        }

        return replaceStagedDirectory(staging, output);
    }

    Materialization materialization;
};

class DscSourceHandler final : public SourceFetchHandler
{
private:
    static constexpr char cachedFileName[] = "source.dsc";

    auto normalizeExpectedHash(std::optional<std::string> expected) const noexcept
      -> utils::error::Result<std::optional<std::string>> override
    {
        LINGLONG_TRACE("validate expected dsc source hash");

        expected = normalizeSha256(std::move(expected));
        if (expected && !isSha256Hash(*expected, "sha256:")) {
            return LINGLONG_ERR("invalid source SHA-256 digest");
        }
        return expected;
    }

    auto acquire(const SourceFetchContext &context,
                 const std::filesystem::path &temporary) const noexcept
      -> utils::error::Result<AcquiredSource> override
    {
        LINGLONG_TRACE("acquire dsc source");

        const auto &source = requestSource(context.request);
        const auto name = fileNameFromUrl(*source.url);
        if (name.empty()) {
            return LINGLONG_ERR("dsc source URL does not contain a file name");
        }

        const auto artifact = temporary / "source";
        auto downloaded =
          runFetchScript("fetch-dsc-source", { "download", *source.url, artifact.string() });
        if (!downloaded) {
            return LINGLONG_ERR(downloaded);
        }

        auto hash = fileHash(artifact / name);
        if (!hash) {
            return LINGLONG_ERR(hash);
        }

        if (name != cachedFileName) {
            std::error_code ec;
            std::filesystem::rename(artifact / name, artifact / cachedFileName, ec);
            if (ec) {
                return LINGLONG_ERR("failed to prepare cached dsc source: " + ec.message());
            }
        }

        const auto resolved = BuilderSourceLockResolved{ .hash = *hash };
        if (!validate(context, artifact, resolved)) {
            return LINGLONG_ERR("downloaded dsc source failed validation");
        }

        return AcquiredSource{ .artifact = artifact,
                               .result = BuilderSourceLockResolved{ .hash = std::move(*hash) } };
    }

    bool validate(const SourceFetchContext &,
                  const std::filesystem::path &cached,
                  const BuilderSourceLockResolved &resolved) const noexcept override
    {
        auto actual = fileHash(cached / cachedFileName);
        if (!actual || *actual != resolved.hash) {
            return false;
        }

        return static_cast<bool>(
          runFetchScript("fetch-dsc-source", { "verify", cached.string(), cachedFileName }));
    }

    auto materialize(const SourceFetchContext &context,
                     const std::filesystem::path &cached,
                     const BuilderSourceLockResolved &) const noexcept
      -> utils::error::Result<void> override
    {
        LINGLONG_TRACE("materialize dsc source");

        auto temporary = utils::TemporaryDirectory::create(".materialize-", context.destination);
        if (!temporary) {
            return LINGLONG_ERR(temporary);
        }

        const auto staging = temporary->path() / "source";
        auto extracted =
          runFetchScript("fetch-dsc-source",
                         { "materialize", cached.string(), cachedFileName, staging.string() });
        if (!extracted) {
            return LINGLONG_ERR(extracted);
        }

        return replaceStagedDirectory(staging,
                                      context.destination
                                        / sourceOutputName(requestSource(context.request)));
    }
};

class GitSourceHandler final : public SourceFetchHandler
{
private:
    auto normalizeExpectedHash(std::optional<std::string> expected) const noexcept
      -> utils::error::Result<std::optional<std::string>> override
    {
        LINGLONG_TRACE("validate expected git source hash");

        if (expected && !gitCommitFromHash(*expected)) {
            return LINGLONG_ERR("invalid git source hash");
        }
        return expected;
    }

    auto acquire(const SourceFetchContext &context,
                 const std::filesystem::path &temporary) const noexcept
      -> utils::error::Result<AcquiredSource> override
    {
        LINGLONG_TRACE("acquire git source");

        const auto &source = requestSource(context.request);
        if (!source.commit) {
            return LINGLONG_ERR("git source requires commit");
        }

        const auto artifact = temporary / "checkout";
        auto ref = *source.commit;
        if (const auto *locked = std::get_if<LockedSourceRequest>(&context.request)) {
            auto commit = gitCommitFromHash(locked->resolved.hash);
            if (!commit) {
                return LINGLONG_ERR("invalid git source hash in project lock");
            }
            ref = std::move(*commit);
        }

        auto checkedOut =
          runFetchScript("fetch-git-source",
                         { "checkout", *source.url, ref, artifact.string() },
                         { { "GIT_SUBMODULES", source.submodules.value_or(true) ? "true" : "" } });
        if (!checkedOut) {
            return LINGLONG_ERR(checkedOut);
        }

        auto commit = lastOutputLine(*checkedOut);
        if (!isGitCommitId(commit)) {
            return LINGLONG_ERR("git source helper did not return a resolved commit");
        }

        LogD("source fetch: checked out git source {} at commit {}",
             sourceOutputName(source),
             commit);
        auto hash = "git-commit:" + commit;

        return AcquiredSource{ .artifact = artifact,
                               .result = BuilderSourceLockResolved{ .hash = std::move(hash) } };
    }

    bool validate(const SourceFetchContext &context,
                  const std::filesystem::path &cached,
                  const BuilderSourceLockResolved &resolved) const noexcept override
    {
        const auto commit = gitCommitFromHash(resolved.hash);
        if (!commit) {
            return false;
        }

        std::error_code ec;
        const auto metadata = std::filesystem::symlink_status(cached / ".git", ec);
        if (ec || !std::filesystem::is_directory(metadata)) {
            return false;
        }

        utils::Cmd git("git");
        if (!git.exists()) {
            return false;
        }
        auto head = git.exec({ "-C", cached.string(), "rev-parse", "--verify", "HEAD^{commit}" });
        if (!head || !sameGitCommit(lastOutputLine(*head), *commit)) {
            return false;
        }

        // The working tree is restored in a temporary copy. Validate the objects instead of
        // relying on status, which can hide ignored files and assume-unchanged modifications.
        if (!git.exec({ "-C", cached.string(), "fsck", "--full", "--no-dangling", *commit })) {
            return false;
        }
        if (!git.exec({ "-C", cached.string(), "remote", "get-url", "origin" })) {
            return false;
        }

        if (!requestSource(context.request).submodules.value_or(true)) {
            return true;
        }

        auto submodules = git.exec({ "-C", cached.string(), "submodule", "status", "--recursive" });
        if (!submodules) {
            return false;
        }

        std::istringstream lines(*submodules);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.front() != ' ') {
                return false;
            }
        }
        return static_cast<bool>(git.exec({ "-C",
                                            cached.string(),
                                            "submodule",
                                            "foreach",
                                            "--recursive",
                                            "git fsck --full --no-dangling \"$sha1\"" }));
    }

    auto materialize(const SourceFetchContext &context,
                     const std::filesystem::path &cached,
                     const BuilderSourceLockResolved &resolved) const noexcept
      -> utils::error::Result<void> override
    {
        LINGLONG_TRACE("materialize git checkout");

        const auto &source = requestSource(context.request);
        const auto destination = context.destination / sourceOutputName(source);

        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec) {
            return LINGLONG_ERR("failed to create source destination: " + ec.message());
        }

        auto temporary =
          utils::TemporaryDirectory::create(".materialize-", destination.parent_path());
        if (!temporary) {
            return LINGLONG_ERR(temporary);
        }

        const auto staging = temporary->path() / "source";
        auto copied = copyTree(cached, staging);
        if (!copied) {
            return LINGLONG_ERR(copied);
        }

        utils::Cmd git("git");
        const auto commit = gitCommitFromHash(resolved.hash);
        if (!commit) {
            return LINGLONG_ERR("invalid git source hash");
        }

        std::vector<std::string> resetArguments{ "-C", staging.string(), "reset", "--hard" };
        if (source.submodules.value_or(true)) {
            resetArguments.emplace_back("--recurse-submodules");
        }
        resetArguments.push_back(*commit);
        auto reset = git.exec(resetArguments);
        if (!reset) {
            return LINGLONG_ERR("failed to reset materialized git source", reset);
        }

        // A hard reset can leave skip-worktree files untouched. Force every tracked file to
        // be restored from the reset index, regardless of its copied index flags or stat data.
        auto restored = git.exec({ "-C",
                                   staging.string(),
                                   "checkout-index",
                                   "--all",
                                   "--force",
                                   "--ignore-skip-worktree-bits" });
        if (!restored) {
            return LINGLONG_ERR("failed to restore materialized git source files", restored);
        }

        auto cleaned = git.exec({ "-C", staging.string(), "clean", "-ffdx" });
        if (!cleaned) {
            return LINGLONG_ERR("failed to clean materialized git source", cleaned);
        }
        if (source.submodules.value_or(true)) {
            auto cleanedSubmodules = git.exec({ "-C",
                                                staging.string(),
                                                "submodule",
                                                "foreach",
                                                "--recursive",
                                                "git checkout-index --all --force "
                                                "--ignore-skip-worktree-bits && git clean -ffdx" });
            if (!cleanedSubmodules) {
                return LINGLONG_ERR("failed to clean materialized git submodules",
                                    cleanedSubmodules);
            }
        }

        if (!validate(context, staging, resolved)) {
            return LINGLONG_ERR("materialized git source failed validation");
        }
        auto status = git.exec({ "-C",
                                 staging.string(),
                                 "status",
                                 "--porcelain=v1",
                                 "--untracked-files=all",
                                 "--ignore-submodules=none",
                                 "--ignored" });
        if (!status || !trim(*status).empty()) {
            return LINGLONG_ERR("materialized git source working tree is not clean");
        }

        auto remote =
          git.exec({ "-C", staging.string(), "remote", "set-url", "origin", *source.url });
        if (!remote) {
            return LINGLONG_ERR("failed to update materialized git remote", remote);
        }
        auto synchronized =
          git.exec({ "-C", staging.string(), "submodule", "sync", "--recursive" });
        if (!synchronized) {
            return LINGLONG_ERR("failed to synchronize materialized git submodules", synchronized);
        }

        return replaceStagedDirectory(staging, destination);
    }

    auto cachePath(const SourceFetchContext &context,
                   const BuilderSourceLockResolved &resolved) const
      -> std::filesystem::path override
    {
        const auto commit = gitCommitFromHash(resolved.hash);
        return context.cache.gitObject(*commit,
                                       requestSource(context.request).submodules.value_or(true));
    }
};

auto createSourceFetchHandler(std::string_view kind) noexcept
  -> utils::error::Result<std::unique_ptr<SourceFetchHandler>>
{
    LINGLONG_TRACE("create source fetch handler");

    std::unique_ptr<SourceFetchHandler> handler;
    if (kind == "file") {
        handler = std::make_unique<BlobSourceHandler>(BlobSourceHandler::Materialization::File);
    } else if (kind == "archive") {
        handler = std::make_unique<BlobSourceHandler>(BlobSourceHandler::Materialization::Archive);
    } else if (kind == "dsc") {
        handler = std::make_unique<DscSourceHandler>();
    } else if (kind == "git") {
        handler = std::make_unique<GitSourceHandler>();
    } else {
        return LINGLONG_ERR("unknown source kind");
    }
    return handler;
}

} // namespace

auto gitCommitFromHash(std::string_view hash) -> std::optional<std::string>
{
    constexpr auto prefix = std::string_view("git-commit:");
    if (hash.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    const auto commit = hash.substr(prefix.size());
    if (!isGitCommitId(commit)) {
        return std::nullopt;
    }
    return std::string(commit);
}

auto SourceFetcher::fetch(const std::filesystem::path &destination, bool offline) noexcept
  -> utils::error::Result<api::types::v1::BuilderSourceLockResolved>
{
    LINGLONG_TRACE("fetch source");

    const auto &source = requestSource(request);
    if (!source.url) {
        return LINGLONG_ERR("source URL is missing");
    }

    const auto name = sourceOutputName(source);
    const std::filesystem::path outputName(name);
    if (name.empty() || name == "." || name == ".." || outputName.is_absolute()
        || outputName.has_parent_path()) {
        return LINGLONG_ERR("source name must be a non-empty filename");
    }

    auto handler = createSourceFetchHandler(source.kind);
    if (!handler) {
        return LINGLONG_ERR(handler);
    }

    SourceFetchContext context{
        .request = request,
        .cache = cache,
        .destination = destination,
        .offline = offline,
    };
    return (*handler)->fetch(context);
}

SourceFetcher::SourceFetcher(SourceFetchRequest request, SourceCacheLayout cache)
    : cache(std::move(cache))
    , request(std::move(request))
{
}

} // namespace linglong::builder
