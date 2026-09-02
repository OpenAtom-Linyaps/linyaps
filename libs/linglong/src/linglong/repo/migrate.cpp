// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "migrate.h"

#include "configure.h"
#include "linglong/package/version.h"
#include "linglong/repo/config.h"
#include "linglong/utils/filelock.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree.h>

#include <QFile>
#include <QSaveFile>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace {

struct MigrateRefData
{
    std::filesystem::path root;
    std::string repoName;
};

struct Version
{
    int major{ 0 };
    int minor{ 0 };
    int patch{ 0 };

    friend bool operator<(const Version &lhs, const Version &rhs) noexcept
    {
        if (lhs.major != rhs.major) {
            return lhs.major < rhs.major;
        }
        if (lhs.minor != rhs.minor) {
            return lhs.minor < rhs.minor;
        }
        return lhs.patch < rhs.patch;
    }
};

struct RepositoryVersion
{
    std::string raw;
    Version parsed;
};

std::mutex migrationMutex;

std::optional<Version> parseVersion(std::string_view version)
try {
    Version v;
    auto p1 = version.find('.');
    if (p1 == std::string::npos) {
        return std::nullopt;
    }
    v.major = std::stoi(std::string{ version.substr(0, p1) });

    auto p2 = version.find('.', p1 + 1);
    if (p2 == std::string::npos) {
        return std::nullopt;
    }
    v.minor = std::stoi(std::string{ version.substr(p1 + 1, p2) });

    if (version.find('.', p2 + 1) != std::string::npos) {
        return std::nullopt;
    }
    v.patch = std::stoi(std::string{ version.substr(p2 + 1, version.size()) });

    return v;
} catch (std::exception &e) {
    std::cerr << e.what() << " cause an exception" << std::endl;
    return std::nullopt;
}

std::optional<RepositoryVersion> readRepositoryVersion(const std::filesystem::path &root)
{
    std::error_code ec;
    const auto versionPath = root / ".version";
    std::optional<std::string> version;
    if (std::filesystem::exists(versionPath, ec)) {
        std::ifstream in{ versionPath };
        if (!in.is_open()) {
            std::cerr << "couldn't open " << versionPath << std::endl;
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << in.rdbuf();
        if (in.bad()) {
            std::cerr << "couldn't read " << versionPath << std::endl;
            return std::nullopt;
        }
        version = buffer.str();
    } else if (ec) {
        std::cerr << "couldn't get status of " << versionPath << std::endl;
        return std::nullopt;
    }

    auto raw = version.value_or("1.5.0");
    auto parsed = parseVersion(raw);
    if (!parsed) {
        std::cerr << "failed to parse repo version " << raw << std::endl;
        return std::nullopt;
    }

    return RepositoryVersion{ .raw = std::move(raw), .parsed = *parsed };
}

std::optional<Version> currentVersion()
{
    auto current = parseVersion(LINGLONG_VERSION);
    if (!current) {
        std::cerr << "failed to parse current version " << LINGLONG_VERSION << std::endl;
    }
    return current;
}

linglong::utils::error::Result<linglong::utils::filelock::FileLock>
lockMigration(const std::filesystem::path &root)
{
    LINGLONG_TRACE("lock repository migration");

    using linglong::utils::filelock::FileLock;
    using linglong::utils::filelock::LockType;

    auto lock = FileLock::create(root / ".migration.lock", LockType::ReadWrite);
    if (!lock) {
        return LINGLONG_ERR(lock);
    }

    auto result = lock->lock(LockType::Write);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return std::move(*lock);
}

bool writeRepositoryVersion(const std::filesystem::path &versionPath)
{
    QSaveFile out{ QFile::decodeName(versionPath.c_str()) };
    out.setDirectWriteFallback(false);
    if (!out.open(QIODevice::WriteOnly)) {
        std::cerr << "couldn't open " << versionPath << ": " << out.errorString().toStdString()
                  << std::endl;
        return false;
    }

    constexpr auto versionLength = sizeof(LINGLONG_VERSION) - 1;
    if (out.write(LINGLONG_VERSION, versionLength) != versionLength) {
        std::cerr << "couldn't write " << versionPath << ": " << out.errorString().toStdString()
                  << std::endl;
        out.cancelWriting();
        return false;
    }

    if (!out.commit()) {
        std::cerr << "couldn't commit " << versionPath << ": " << out.errorString().toStdString()
                  << std::endl;
        return false;
    }

    return true;
}

linglong::repo::MigrateResult checkCompatibility(const RepositoryVersion &repositoryVersion,
                                                 const Version &current)
{
    if (current < repositoryVersion.parsed) {
        std::cerr << "repository version " << repositoryVersion.raw
                  << " is newer than current program version " << LINGLONG_VERSION
                  << ", refusing to migrate" << std::endl;
        return linglong::repo::MigrateResult::Incompatible;
    }

    return linglong::repo::MigrateResult::NoChange;
}

int migrateRef(OstreeRepo *repo, const MigrateRefData &data)
{
    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GHashTable) refsTable{ nullptr };
    if (ostree_repo_list_refs(repo, nullptr, &refsTable, nullptr, &gErr) == FALSE) {
        std::cerr << "couldn't list refs in repo: " << gErr->message << std::endl;
        return -1;
    }

    std::unordered_map<std::string_view, std::string_view> allRefs;
    g_hash_table_foreach(
      refsTable,
      [](gpointer key, gpointer value, gpointer data) { // NOLINT
          auto &refs = *static_cast<std::unordered_map<std::string_view, std::string_view> *>(data);
          refs.emplace(static_cast<const char *>(key), static_cast<const char *>(value));
      },
      &allRefs);

    if (allRefs.empty()) {
        return 0;
    }

    std::unordered_map<std::string_view, std::string_view> needMigrate;
    auto refPrefix = data.repoName + ":";
    for (auto it = allRefs.begin(); it != allRefs.end();) {
        if (it->first.rfind(refPrefix, 0) == 0) {
            ++it;
        } else {
            needMigrate.emplace(it->first, it->second);
            it = allRefs.erase(it);
        }
    }

    for (auto it = needMigrate.begin(); it != needMigrate.end();) {
        auto tmpRef = refPrefix.append(it->first);
        if (allRefs.find(tmpRef) != allRefs.end()) {
            it = needMigrate.erase(it);
        } else {
            ++it;
        }
    }

    if (needMigrate.empty()) {
        return 0;
    }

    if (ostree_repo_prepare_transaction(repo, nullptr, nullptr, &gErr) == FALSE) {
        std::cerr << "failed to prepare transaction:" << gErr->message << std::endl;
        return -1;
    }

    for (auto [ref, checksum] : needMigrate) {
        ostree_repo_transaction_set_ref(repo, data.repoName.c_str(), ref.data(), checksum.data());
    }

    if (ostree_repo_commit_transaction(repo, nullptr, nullptr, &gErr) == 0) {
        std::cerr << "failed to commit transaction:" << gErr->message << std::endl;
        return -1;
    }

    std::error_code ec;
    auto layers = data.root / "layers";
    if (!std::filesystem::exists(layers, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << layers << std::endl;
        }

        std::cerr << "layers not found: " << layers << std::endl;
        return -1;
    }

    for (auto [ref, checksum] : needMigrate) {
        std::string realRef{ ref };
        auto lastSlash = ref.find_last_of('/');
        if (lastSlash == std::string::npos) {
            std::cerr << "failed to get last slash in " << ref << std::endl;
            continue;
        }

        auto module = ref.substr(lastSlash + 1);
        auto oldLayerPath = layers / ref;
        if (!std::filesystem::exists(oldLayerPath, ec)) {
            if (ec) {
                std::cerr << "couldn't get status of layer directory " << oldLayerPath << std::endl;
                continue;
            }

            if (module == "runtime") {
                auto fallback = std::string{ ref.substr(0, lastSlash) } + "/binary";
                auto fallbackPath = layers / fallback;
                if (!std::filesystem::exists(fallbackPath, ec)) {
                    if (ec) {
                        std::cerr << "couldn't get status of layer directory " << fallbackPath
                                  << std::endl;
                    }

                    continue;
                }

                realRef = std::move(fallback);
            } else {
                continue;
            }
        }

        auto newLayerPath = layers / checksum;
        std::filesystem::create_symlink(realRef, newLayerPath, ec);
        if (ec && ec != std::errc::file_exists) {
            std::cerr << "couldn't create symlink from " << oldLayerPath << " to " << newLayerPath
                      << std::endl;
            return -1;
        }
    }

    return 1;
}

int dispatchMigrations(const Version &from,
                       const std::filesystem::path &root,
                       const linglong::api::types::v1::RepoConfigV2 &cfg)
{
    std::error_code ec;
    std::filesystem::path ostreeRepo = root / "repo";
    if (!std::filesystem::exists(ostreeRepo, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << ostreeRepo << std::endl;
        }

        return 0;
    }

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) repoPath = nullptr;
    g_autoptr(OstreeRepo) repo = nullptr;

    repoPath = g_file_new_for_path(ostreeRepo.c_str());
    repo = ostree_repo_new(repoPath);
    if (ostree_repo_open(repo, nullptr, &gErr) == FALSE) {
        std::cerr << "couldn't open repo " << ostreeRepo << ":" << gErr->message << std::endl;
        return -1;
    }

    int ret{ std::numeric_limits<int>::max() };
    auto version_1_7_0 = parseVersion("1.7.0");
    if (from < *version_1_7_0) {
        const auto &defaultRepo = linglong::repo::getDefaultRepo(cfg);
        ret = migrateRef(repo, MigrateRefData{ .root = root, .repoName = defaultRepo.name });
    }

    return ret;
}

} // namespace

namespace linglong::repo {
MigrateResult checkRepoCompatibility(const std::filesystem::path &root) noexcept
{
    std::lock_guard processLock{ migrationMutex };

    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << root << std::endl;
            return MigrateResult::Failed;
        }
        return MigrateResult::NoChange;
    }

    auto migrationLock = lockMigration(root);
    if (!migrationLock) {
        std::cerr << "couldn't lock repository " << root << ": " << migrationLock.error().message()
                  << std::endl;
        return MigrateResult::Failed;
    }

    auto repositoryVersion = readRepositoryVersion(root);
    auto current = currentVersion();
    if (!repositoryVersion || !current) {
        return MigrateResult::Failed;
    }

    return checkCompatibility(*repositoryVersion, *current);
}

MigrateResult tryMigrate(const std::filesystem::path &root,
                         const linglong::api::types::v1::RepoConfigV2 &cfg) noexcept
{
    std::lock_guard processLock{ migrationMutex };

    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << root << std::endl;
            return MigrateResult::Failed;
        }

        return MigrateResult::NoChange;
    }

    auto migrationLock = lockMigration(root);
    if (!migrationLock) {
        std::cerr << "couldn't lock repository " << root << ": " << migrationLock.error().message()
                  << std::endl;
        return MigrateResult::Failed;
    }

    auto repositoryVersion = readRepositoryVersion(root);
    auto current = currentVersion();
    if (!repositoryVersion || !current) {
        return MigrateResult::Failed;
    }

    if (repositoryVersion->raw == LINGLONG_VERSION) {
        return MigrateResult::NoChange;
    }

    auto compatibility = checkCompatibility(*repositoryVersion, *current);
    if (compatibility == MigrateResult::Incompatible) {
        return compatibility;
    }

    auto ret = dispatchMigrations(repositoryVersion->parsed, root, cfg);
    if (ret == -1) {
        return MigrateResult::Failed;
    }

    auto result = ret == 0 ? MigrateResult::NoChange : MigrateResult::Success;

    if (!writeRepositoryVersion(root / ".version")) {
        return MigrateResult::Failed;
    }

    return result;
}
} // namespace linglong::repo
