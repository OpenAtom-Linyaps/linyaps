// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace linglong::cli {

struct DoctorCheckResult
{
    std::string name;      // short identifier of the checked component
    bool required{ true }; // failing a required check makes the environment unusable
    bool ok{ false };
    std::string detail; // finding or hint for the user
};

// Run every local environment check and return the results in a stable order.
std::vector<DoctorCheckResult> runDoctorChecks();

// Compare two "MAJOR.MINOR.PATCH" version strings, ignoring any suffix after '-'.
// Returns a positive value when lhs is newer, a negative value when lhs is older,
// zero when both are equal, and nullopt when either side cannot be parsed.
std::optional<int> compareVersions(const std::string &lhs, const std::string &rhs);

} // namespace linglong::cli
