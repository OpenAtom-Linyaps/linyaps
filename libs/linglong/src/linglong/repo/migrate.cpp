// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "migrate.h"

#include "linglong/package/version.h"
#include "linglong/utils/configure.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>

struct MigrateRefData
{
    std::string repoName;
};

struct Version
{
    int major{ 0 };
    int minor{ 0 };
    int patch{ 0 };

    friend bool operator<(const Version &lhs, const Version &rhs) noexcept
    {
        return lhs.major < rhs.major || lhs.minor < rhs.minor || lhs.patch < rhs.patch;
    }
};

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

void migrateRef(OstreeRepo *repo, const MigrateRefData &data)
{
    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GHashTable) refsTable{ nullptr };
    if (ostree_repo_list_refs(repo, nullptr, &refsTable, nullptr, &gErr) == FALSE) {
        std::cerr << "couldn't list refs in repo: " << gErr->message << std::endl;
        return;
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
        std::cout << "underlying repo is empty, no need to migrate." << std::endl;
        return;
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
        std::cout << "all refs have been migrated, no need to migrate." << std::endl;
        return;
    }

    if (ostree_repo_prepare_transaction(repo, nullptr, nullptr, &gErr) == FALSE) {
        std::cerr << "failed to prepare transaction:" << gErr->message << std::endl;
        return;
    }

    for (auto [ref, checksum] : needMigrate) {
        ostree_repo_transaction_set_ref(repo, data.repoName.c_str(), ref.data(), checksum.data());
    }

    if (ostree_repo_commit_transaction(repo, nullptr, nullptr, &gErr) == 0) {
        std::cerr << "failed to commit transaction:" << gErr->message << std::endl;
        return;
    }

    std::error_code ec;
    auto layers = std::filesystem::path{ LINGLONG_ROOT } / "layers";
    if (!std::filesystem::exists(layers, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << layers << std::endl;
        }

        return;
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
            return;
        }
    }
}

void dispatchMigrations(const Version &from)
{
    std::error_code ec;
    auto ostreeRepo = std::filesystem::path{ LINGLONG_ROOT } / "repo";
    if (!std::filesystem::exists(ostreeRepo, ec)) {
        if (ec) {
            std::cerr << "couldn't get status of " << ostreeRepo << std::endl;
        }
        return;
    }

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) repoPath = nullptr;
    g_autoptr(OstreeRepo) repo = nullptr;

    repoPath = g_file_new_for_path(ostreeRepo.c_str());
    repo = ostree_repo_new(repoPath);
    if (ostree_repo_open(repo, nullptr, &gErr) == FALSE) {
        std::cerr << "couldn't open repo " << ostreeRepo << ":" << gErr->message << std::endl;
        return;
    }

    auto version_1_7_0 = parseVersion("1.7.0");
    if (from < *version_1_7_0) {
        migrateRef(repo, MigrateRefData{ .repoName = "stable" });
    }
}

namespace linglong::repo {
void tryMigrate() noexcept
{
    std::error_code ec;
    std::optional<std::string> repoVersion;
    std::filesystem::path version = std::filesystem::path{ LINGLONG_ROOT } / ".version";
    if (std::filesystem::exists(version, ec)) {
        std::ifstream in{ version };
        if (!in.is_open()) {
            std::cerr << "couldn't open " << version << std::endl;
        } else {
            std::stringstream buffer;
            buffer << in.rdbuf();
            repoVersion = buffer.str();
        }
    }

    auto repoVer = repoVersion.value_or("1.5.0");
    if (repoVer == LINGLONG_VERSION) {
        return;
    }

    auto from = parseVersion(repoVer);
    if (!from) {
        std::cerr << "failed to parse repo version " << repoVer << std::endl;
        return;
    }

    dispatchMigrations(*from);

    std::ofstream out;
    out.open(version, std::ios_base::out | std::ios_base::trunc);
    if (!out.is_open()) {
        std::cerr << "couldn't open " << version << std::endl;
    } else {
        out << LINGLONG_VERSION;
    }
    out.clear();
}
} // namespace linglong::repo
