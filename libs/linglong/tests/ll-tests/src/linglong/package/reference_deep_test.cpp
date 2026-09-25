/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"

#include <string>
#include <vector>

namespace linglong::package {
namespace {

TEST(ReferenceDeepTest, ReferenceParsingAndStringFormat)
{
    auto ref = Reference::parse("org.deepin.demo/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();
    EXPECT_EQ(ref->id(), "org.deepin.demo");
    EXPECT_EQ(ref->version(), "1.0.0");
    EXPECT_EQ(ref->arch(), "x86_64");
}

TEST(ReferenceDeepTest, ReferenceComparisonAndHashing)
{
    auto r1 = Reference::parse("org.deepin.demo/1.0.0/x86_64");
    auto r2 = Reference::parse("org.deepin.demo/1.0.0/x86_64");
    auto r3 = Reference::parse("org.deepin.demo/1.0.1/x86_64");

    ASSERT_TRUE(r1 && r2 && r3);
    EXPECT_EQ(*r1, *r2);
    EXPECT_NE(*r1, *r3);
}

TEST(ReferenceDeepTest, FuzzyReferenceMatchingLogic)
{
    FuzzyReference fuzzy("org.deepin.demo");
    EXPECT_TRUE(fuzzy.match("org.deepin.demo/1.0.0/x86_64"));
    EXPECT_FALSE(fuzzy.match("org.deepin.other/1.0.0/x86_64"));
}

} // namespace
} // namespace linglong::package
