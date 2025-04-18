/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

#include <sstream>
using namespace semver;

TEST(VersionTest, OstreamOperator)
{
    version v = version::parse("5.2.3");
    std::stringstream ss;
    ss << v;
    EXPECT_EQ(ss.str(), "5.2.3");
}

#ifndef SEMVER_TEST_MODULE
#  ifdef __cpp_lib_format
#    if __cpp_lib_format >= 201907L

TEST(VersionTest, StdFormatFormatter)
{
    EXPECT_EQ(std::format("{}", version::parse("5.2.3")), "5.2.3");
    EXPECT_EQ(std::format("{}", version::parse("5.2.3-alpha")), "5.2.3-alpha");
    EXPECT_EQ(std::format("{}", version::parse("5.2.3-1.2.3")), "5.2.3-1.2.3");
    EXPECT_EQ(std::format("{}", version::parse("5.2.3-alpha+build34")), "5.2.3-alpha+build34");
    EXPECT_EQ(std::format("{}", version::parse("5.2.3-1.2.3+build34")), "5.2.3-1.2.3+build34");
}

#    endif
#  endif
#endif
