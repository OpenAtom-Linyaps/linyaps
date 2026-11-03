// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/extension/extension.h"

#include <string>

namespace {

using linglong::extension::ExtensionFactory;
using linglong::extension::ExtensionImplDummy;
using linglong::extension::ExtensionImplNVIDIADisplayDriver;

TEST(ExtensionTest, UnknownNameYieldsDummy)
{
    auto extension = ExtensionFactory::makeExtension("org.example.unknown");
    ASSERT_NE(extension, nullptr);

    std::string name = "anything";
    EXPECT_TRUE(extension->shouldEnable(name));
    EXPECT_FALSE(extension->shouldDownload(name));
    EXPECT_FALSE(extension->shouldPrune(name));
}

TEST(ExtensionTest, NvidiaIdentifyYieldsNvidiaDriver)
{
    const std::string identify = ExtensionImplNVIDIADisplayDriver::Identify;
    auto extension = ExtensionFactory::makeExtension(identify);
    ASSERT_NE(extension, nullptr);

    std::string wrong = "org.example.other";
    EXPECT_FALSE(extension->shouldEnable(wrong));

    std::string candidate = identify;
    bool enabled = extension->shouldEnable(candidate);
    if (enabled) {
        EXPECT_TRUE(candidate.rfind(identify, 0) == 0);
    }
}

TEST(ExtensionTest, DummyExtensionAlwaysEnables)
{
    ExtensionImplDummy dummy;
    std::string name = "anything";
    EXPECT_TRUE(dummy.shouldEnable(name));
    EXPECT_FALSE(dummy.shouldDownload(name));
    EXPECT_FALSE(dummy.shouldPrune(name));
}

} // namespace
