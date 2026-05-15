/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/utils/xdp.h"

using namespace linglong::utils;

TEST(IsValidXdgDesktopPortalIdTest, ValidStandardId)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example.app"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("com.deepin.calculator"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.freedesktop.portal"));
}

TEST(IsValidXdgDesktopPortalIdTest, ValidWithHyphenInLastSegment)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example.my-app"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("com.deepin.app-launcher"));
}

TEST(IsValidXdgDesktopPortalIdTest, ValidWithUnderscore)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example_my.app"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("com.deepin.my_app"));
}

TEST(IsValidXdgDesktopPortalIdTest, ValidWithDigits)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example.app2"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.2example.app"));
}

TEST(IsValidXdgDesktopPortalIdTest, ValidMultiSegment)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.deepin.linglong.app"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("a.b.c.d.e"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidSingleSegment)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("single"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("app"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidEmptyString)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId(""));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidHyphenInNonLastSegment)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.my-app.app"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("my-org.example.app"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidEmptySegment)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("org..app"));
    EXPECT_FALSE(isValidXdgDesktopPortalId(".org.app"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.app."));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidSpecialChars)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.example.app!"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.example.app@home"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("org/example/app"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidSlashInId)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.deepin.calculator/stable"));
    EXPECT_FALSE(isValidXdgDesktopPortalId("org/deepin/app"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidHyphenInMultipleNonLastSegments)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("my-org.my-app.app"));
}

TEST(IsValidXdgDesktopPortalIdTest, ValidHyphenOnlyInLastSegment)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example.a-b"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.example.a-b-c"));
}

TEST(IsValidXdgDesktopPortalIdTest, InvalidLastSegmentEmpty)
{
    EXPECT_FALSE(isValidXdgDesktopPortalId("org.example."));
}

TEST(IsValidXdgDesktopPortalIdTest, TwoSegmentsValid)
{
    EXPECT_TRUE(isValidXdgDesktopPortalId("org.app"));
    EXPECT_TRUE(isValidXdgDesktopPortalId("a.b"));
}
