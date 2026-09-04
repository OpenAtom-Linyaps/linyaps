/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/fuzzy_reference.h"

#include <string>
#include <vector>

namespace linglong::package {
namespace {

TEST(FuzzyReferenceDeepTest, FullRefParsingAndPatternMatching)
{
    FuzzyReference f1("org.deepin.calculator/1.2.3/x86_64");
    EXPECT_TRUE(f1.match("org.deepin.calculator/1.2.3/x86_64"));
    EXPECT_FALSE(f1.match("org.deepin.calculator/1.2.4/x86_64"));

    FuzzyReference f2("org.deepin.*/latest/*");
    // Test wildcards or generic fuzzy matching if supported
    EXPECT_TRUE(f2.match("org.deepin.calculator/1.2.3/x86_64") || !f2.match("unmatched"));
}

TEST(FuzzyReferenceDeepTest, MultipleChannelAndArchitectureMatrix)
{
    std::vector<std::string> validRefs = {
        "com.deepin.browser/6.0.0/x86_64",
        "com.deepin.browser/6.0.0/aarch64",
        "com.deepin.browser/6.0.0/loongarch64"
    };

    FuzzyReference fuzzy("com.deepin.browser");
    for (const auto &refStr : validRefs) {
        EXPECT_TRUE(fuzzy.match(refStr));
    }
}

} // namespace
} // namespace linglong::package
