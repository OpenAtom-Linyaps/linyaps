// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/builder/project_validation.h"

namespace linglong::builder {

TEST(ProjectValidationTest, PackageIdAcceptsValidXdgDesktopPortalIds)
{
    for (const auto *packageId : { "org.example.app",
                                   "com.deepin.calculator",
                                   "org.freedesktop.portal",
                                   "org.example.my-app",
                                   "com.deepin.app-launcher",
                                   "org.example_my.app",
                                   "com.deepin.my_app",
                                   "org.example.app2",
                                   "org.2example.app",
                                   "org.deepin.linglong.app",
                                   "a.b.c.d.e" }) {
        EXPECT_TRUE(validatePackageId(packageId)) << packageId;
    }
}

TEST(ProjectValidationTest, PackageIdRejectsInvalidXdgDesktopPortalIds)
{
    for (const auto *packageId : { "",
                                   "single",
                                   "app",
                                   "org..app",
                                   ".org.app",
                                   "org.app.",
                                   "org.my-app.app",
                                   "my-org.example.app",
                                   "org.example.app!",
                                   "org.example.app@home",
                                   "org/example/app",
                                   "org.deepin.calculator/stable",
                                   "org.example. app",
                                   "org.\u4f8b.app" }) {
        auto result = validatePackageId(packageId);
        ASSERT_FALSE(result) << packageId;
        EXPECT_NE(result.error().message().find("package.id"), std::string::npos) << packageId;
    }
}

TEST(ProjectValidationTest, PackageIdReportsEmptyValue)
{
    auto result = validatePackageId("");

    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("package.id must not be empty"), std::string::npos);
}

TEST(ProjectValidationTest, ModuleNameAcceptsAllowedCharacters)
{
    for (const auto *moduleName : { "binary",
                                    "develop",
                                    "main",
                                    "translations",
                                    "extra_data",
                                    "extra-data",
                                    "lang-zh_CN",
                                    "module1",
                                    "A_B_9",
                                    "1abc",
                                    "_abc",
                                    "-abc",
                                    "abc_1",
                                    "ABC",
                                    "_",
                                    "-",
                                    "x" }) {
        EXPECT_TRUE(validateModuleName(moduleName)) << moduleName;
    }
}

TEST(ProjectValidationTest, ModuleNameRejectsInvalidCharacters)
{
    for (const auto *moduleName :
         { "", ".", "a.b", "a b", " ", "a/b", "/", "a+b", "+", "module@1", "\u6a21\u5757" }) {
        auto result = validateModuleName(moduleName);
        ASSERT_FALSE(result) << moduleName;
        EXPECT_NE(result.error().message().find("module name"), std::string::npos) << moduleName;
    }
}

TEST(ProjectValidationTest, ModuleNameAcceptsLeadingCharactersWithoutRestriction)
{
    EXPECT_TRUE(validateModuleName("1abc"));
    EXPECT_TRUE(validateModuleName("_abc"));
    EXPECT_TRUE(validateModuleName("-abc"));
}

TEST(ProjectValidationTest, ModuleNameReportsEmptyValue)
{
    auto result = validateModuleName("");

    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("module name must not be empty"), std::string::npos);
}

} // namespace linglong::builder
