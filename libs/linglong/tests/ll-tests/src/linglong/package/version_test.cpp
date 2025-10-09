/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/version.h"

using namespace linglong::package;

TEST(Packge, Version)
{
    // 1.0.0.0 > 1.0.0+security.1 > 1.0.0 > 1.0.0-alpha
    EXPECT_GT(Version::parse("1.0.0.0").value(), Version::parse("1.0.0+security.1").value());
    EXPECT_GT(Version::parse("1.0.0.0").value(), Version::parse("1.0.0").value());
    EXPECT_GT(Version::parse("1.0.0.0").value(), Version::parse("1.0.0-aplha").value());
    EXPECT_GT(Version::parse("1.0.0+security.1").value(), Version::parse("1.0.0").value());
    EXPECT_GT(Version::parse("1.0.0+security.1").value(), Version::parse("1.0.0-alpha").value());
    // 1.0.0-alpha < 1.0.0-beta < 1.0.0 = 1.0.0+zbuild < 1.0.0+security.1 < 1.0.0+security.2
    EXPECT_LT(Version::parse("1.0.0-alpha").value(), Version::parse("1.0.0-beta").value());
    EXPECT_LT(Version::parse("1.0.0-alpha").value(), Version::parse("1.0.0").value());
    EXPECT_LT(Version::parse("1.0.0-beta").value(), Version::parse("1.0.0").value());
    EXPECT_LT(Version::parse("1.0.0+zbuild").value(), Version::parse("1.0.0+security.1").value());
    EXPECT_LT(Version::parse("1.0.0+security.1").value(),
              Version::parse("1.0.0+security.2").value());
}

TEST(Package, VersionRegex101)
{
    // tests modified from https://regex101.com/r/vkijKf/1/

    const std::vector<std::string> validCases = {
        "0.0.0.4",
        "1.2.3.4",
        "10.20.30.40",
        "1.0.0.0",
        "2.0.0.0",
        "1.1.1.7",
        "999999999999999999.999999999999999999.99999999999999999.99999999999999999",

        // 新增的语义化版本号测试用例
        "1.0.0",
        "1.0.0-alpha",
        "1.0.0+build.1",
        "1.0.0-alpha+build.1",
        "1.0.0+buildinfo.security.1",
        "1.0.0-alpha+buildinfo.security.1",
        "1.0.0+security.2",
        "1.0.0+buildinfo.security.3",
    };

    for (const auto &validCase : validCases) {
        auto res = Version::parse(validCase);
        ASSERT_EQ(res.has_value(), true) << validCase << " is valid.";
        ASSERT_EQ(res->toString(), validCase);
    }
}

TEST(Package, VersionCompare)
{
    std::vector<std::string> versions = {
        "1.0.0-alpha",      "1.0.0-beta", "1.0.0-rc", "1.0.0+buildinfo.security.1",
        "1.0.0+security.2", "1.0.0.0",    "1.0.0.1",  "2.0.0.0",
        "2.1.0.0",          "2.1.1.0",    "2.1.1.1",  "3.1.6"
    };
    for (std::size_t i = 0; i < versions.size() - 1; i++) {
        for (std::size_t j = i + 1; j < versions.size(); j++) {
            auto x = Version::parse(versions[i]);
            ASSERT_EQ(x.has_value(), true) << versions[i] << " is valid.";
            auto y = Version::parse(versions[j]);
            ASSERT_EQ(y.has_value(), true) << versions[j] << " is valid.";

            ASSERT_EQ(*x < *y, true) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*x <= *y, true) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*y < *x, false) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*y <= *x, false) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*x != *y, true) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*y > *x, true) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*x > *y, false) << "x: " << x->toString() << " y:" << y->toString();
            ASSERT_EQ(*x >= *y, false) << "x: " << x->toString() << " y:" << y->toString();
        }
    }
}
