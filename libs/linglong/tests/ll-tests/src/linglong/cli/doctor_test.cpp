// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/cli/doctor.h"

#include <algorithm>
#include <set>
#include <string>

namespace linglong::cli {

TEST(DoctorTest, compareVersions)
{
    EXPECT_EQ(compareVersions("1.15.0", "1.15.0"), 0);
    EXPECT_GT(*compareVersions("2.0.0", "1.99.99"), 0);
    EXPECT_LT(*compareVersions("1.14.0", "1.15.0"), 0);
    EXPECT_GT(*compareVersions("1.15.1", "1.15.0"), 0);
    EXPECT_LT(*compareVersions("1.9.0", "1.10.0"), 0);

    // suffixes after '-' are ignored
    EXPECT_EQ(compareVersions("1.15.0-dev+1fc6f59", "1.15.0"), 0);
    EXPECT_GT(*compareVersions("1.15.0-dev", "1.14.0"), 0);

    // missing components are treated as zero
    EXPECT_EQ(compareVersions("1.15", "1.15.0"), 0);
    EXPECT_GT(*compareVersions("1.15.1", "1.15"), 0);

    // unparsable input
    EXPECT_FALSE(compareVersions("banana", "1.0.0").has_value());
    EXPECT_FALSE(compareVersions("1.0.0", "").has_value());
    EXPECT_FALSE(compareVersions("1.x.0", "1.0.0").has_value());
}

TEST(DoctorTest, runDoctorChecksProducesWellFormedResults)
{
    auto checks = runDoctorChecks();
    ASSERT_FALSE(checks.empty());

    // every check reports itself with a unique name and a user-facing hint
    std::set<std::string> names;
    for (const auto &check : checks) {
        EXPECT_FALSE(check.name.empty());
        EXPECT_TRUE(names.insert(check.name).second) << "duplicate check name: " << check.name;
        EXPECT_FALSE(check.detail.empty()) << "check without detail: " << check.name;
    }

    // the doctor always reports the components the sandbox depends on
    const auto contains = [&checks](const std::string &name) {
        return std::any_of(checks.cbegin(), checks.cend(), [&name](const auto &check) {
            return check.name == name;
        });
    };
    EXPECT_TRUE(contains("oci-runtime"));
    EXPECT_TRUE(contains("repository"));
    EXPECT_TRUE(contains("overlayfs"));
    EXPECT_TRUE(contains("erofs-tools"));
}

TEST(DoctorTest, runDoctorChecksOptionalFindingsDoNotFail)
{
    auto checks = runDoctorChecks();

    // a check that is not required never turns the environment unusable
    for (const auto &check : checks) {
        if (!check.required) {
            EXPECT_TRUE(check.ok) << "optional check failed: " << check.name;
        }
    }
}

} // namespace linglong::cli
