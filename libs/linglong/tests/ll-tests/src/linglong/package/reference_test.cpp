/*
 ; SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"

using namespace linglong::package;

TEST(Package, Reference)
{
    const std::vector<std::string> validReferences = {
        "main:com.example.App/1.0.0.0/x86_64",
        "some_channel:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0/arm64",
        "main:com.example.App/1.0.0.1/arm64",
        "main:1111/1.0.0.0/x86_64",
        "main:2222/1.0.0.0/x86_64",
        "main:3333/1.0.0.0/arm64",
        "main:4444/1.0.0.1/arm64",
    };

    for (const auto &validCase : validReferences) {
        auto refer = Reference::parse(validCase);
        ASSERT_EQ(refer.has_value(), true)
          << validCase << " is valid reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
        ASSERT_EQ(refer->toString(), validCase);
    }

    const QStringList invalidReferences = {
        "main:com.example.App//1.0.0.0/x86_64",
        "main:1111/1.0.0.0/ x86_64",
        "main:2222/1.0.0.0/unknown",
        ":1.0.0.1-beta/arm64",
        ":com.example.App/1.0.0.0/x86_64",
    };

    for (const auto &invalidCase : invalidReferences) {
        auto refer = Reference::parse(invalidCase);
        ASSERT_EQ(refer.has_value(), false)
          << invalidCase.toStdString() << " is invalid reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
    }

    const std::unordered_map<std::string, std::string> validFuzzReferences = {
        { "unknown:com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/unknown/x86_64", "unknown:com.example.App/unknown/x86_64" },
        { "com.example.App/1.0.0.0/unknown", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App/1.0.0.0", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App", "unknown:com.example.App/unknown/unknown" },
        { "com.example.App/1.0.0.1", "unknown:com.example.App/1.0.0.1/unknown" },
        { "3333/1.0.0.0/arm64", "unknown:3333/1.0.0.0/arm64" },
        { "4444/1.0.0.1/arm64", "unknown:4444/1.0.0.1/arm64" },
    };

    for (const auto &validCase : validFuzzReferences) {
        auto refer = FuzzyReference::parse(validCase.first);
        ASSERT_EQ(refer.has_value(), true)
          << validCase.first << " is valid fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
        ASSERT_EQ(refer->toString(), validCase.second);
    }
}
