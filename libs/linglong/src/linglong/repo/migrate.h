// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/RepoConfigV2.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace linglong::repo {
enum class MigrateResult { Success, Failed, NoChange };

namespace detail {
/// A dotted repo version. Only the three numeric components the .version file
/// carries are modelled, which is all the migration dispatch needs.
struct Version
{
    int major{ 0 };
    int minor{ 0 };
    int patch{ 0 };

    /// Lexicographic by component. Comparing each component independently and
    /// OR-ing the results is not a version order: it makes 2.0.0 < 1.7.0 true
    /// because 0 < 7, so every repo newer than 1.7.0 re-ran the 1.7.0
    /// migration.
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

    friend bool operator==(const Version &lhs, const Version &rhs) noexcept
    {
        return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
    }
};

/// Parse "MAJOR.MINOR.PATCH"; anything else yields nullopt.
std::optional<Version> parseVersion(std::string_view version);
} // namespace detail

MigrateResult tryMigrate(const std::filesystem::path &root,
                         const linglong::api::types::v1::RepoConfigV2 &cfg) noexcept;
} // namespace linglong::repo
