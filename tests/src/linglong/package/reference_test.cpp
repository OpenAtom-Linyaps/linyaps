/*
 ; SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/fuzz_reference.h"
#include "linglong/package/reference.h"

using namespace linglong::package;

TEST(Package, Reference)
{
    QStringList vaildReferences = {
        "com.example.App/1.0.0.0/x86_64/runtime/main",
        "com.example.App/1.0.0.0/x86_64/runtime/some_channel",
        "com.example.App/1.0.0.0/x86_64/develop/main",
        "com.example.App/1.0.0.0-alpha/arm64/runtime/main",
        "com.example.App/1.0.0.1-beta/arm64/runtime/main",
        "1111/1.0.0.0/x86_64/runtime/main",
        "2222/1.0.0.0/x86_64/develop/main",
        "3333/1.0.0.0-alpha/arm64/runtime/main",
        "4444/1.0.0.1-beta/arm64/runtime/main",
    };

    for (const auto &vaildCase : vaildReferences) {
        auto refer = Reference::parse(vaildCase);
        ASSERT_EQ(refer.has_value(), true)
          << vaildCase.toStdString() << " is vaild reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
        ASSERT_EQ(refer->toString().toStdString(), vaildCase.toStdString());
    }

    QStringList invaildReferences = {
        "com.example.App//1.0.0.0/x86_64/runtime/main",
        "com.example.App/1.0.0.0/x86_64/dev/main",
        "com.example.App/1.0.0.-alpha/arm64/runtime/main",
        "com.example.App/1.0.0.1-beta/arm64/main",
        "1111/1.0.0.0/ x86_64/runtime/main",
        "2222/1.0.0.0/unknown/develop/main",
        "3333/1.0.0.0-()/arm64/runtime/main",
        "4444/1.0.0.1-beta/arm64/run/main",
        "/1.0.0.1-beta/arm64/runtime/main",
    };

    for (const auto &invaildCase : invaildReferences) {
        auto refer = Reference::parse(invaildCase);
        ASSERT_EQ(refer.has_value(), false)
          << invaildCase.toStdString() << " is invaild reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
    }

    QList<QPair<QString, QString>> vaildFuzzReferences = {
        { "com.example.App/1.0.0.0/x86_64/runtime/main",
          "com.example.App/1.0.0.0/x86_64/runtime/main" },
        { "com.example.App/1.0.0.0/x86_64/runtime",
          "com.example.App/1.0.0.0/x86_64/runtime/unknown" },
        { "com.example.App/unknown/x86_64/runtime",
          "com.example.App/unknown/x86_64/runtime/unknown" },
        { "com.example.App/1.0.0.0/unknown/runtime",
          "com.example.App/1.0.0.0/unknown/runtime/unknown" },
        { "com.example.App/1.0.0.0/x86_64/unknown",
          "com.example.App/1.0.0.0/x86_64/unknown/unknown" },
        { "com.example.App/1.0.0.0/x86_64", "com.example.App/1.0.0.0/x86_64/unknown/unknown" },
        { "com.example.App/1.0.0.0", "com.example.App/1.0.0.0/unknown/unknown/unknown" },
        { "com.example.App", "com.example.App/unknown/unknown/unknown/unknown" },
        { "com.example.App/1.0.0.0/x86_64/develop",
          "com.example.App/1.0.0.0/x86_64/develop/unknown" },
        { "com.example.App/1.0.0.0/x86_64", "com.example.App/1.0.0.0/x86_64/unknown/unknown" },
        { "com.example.App/1.0.0.0-alpha/arm64/runtime",
          "com.example.App/1.0.0.0-alpha/arm64/runtime/unknown" },
        { "com.example.App/1.0.0.0-alpha/arm64",
          "com.example.App/1.0.0.0-alpha/arm64/unknown/unknown" },
        { "com.example.App/1.0.0.0-alpha",
          "com.example.App/1.0.0.0-alpha/unknown/unknown/unknown" },
        { "com.example.App/1.0.0.1-beta/arm64/runtime",
          "com.example.App/1.0.0.1-beta/arm64/runtime/unknown" },
        { "com.example.App/1.0.0.1-beta/arm64",
          "com.example.App/1.0.0.1-beta/arm64/unknown/unknown" },
        { "com.example.App/1.0.0.1-beta", "com.example.App/1.0.0.1-beta/unknown/unknown/unknown" },
        { "1111/1.0.0.0/x86_64/runtime", "1111/1.0.0.0/x86_64/runtime/unknown" },
        { "2222/1.0.0.0/x86_64/develop", "2222/1.0.0.0/x86_64/develop/unknown" },
        { "3333/1.0.0.0-alpha/arm64/runtime", "3333/1.0.0.0-alpha/arm64/runtime/unknown" },
        { "4444/1.0.0.1-beta/arm64/runtime", "4444/1.0.0.1-beta/arm64/runtime/unknown" },
    };

    for (const auto &vaildCase : vaildFuzzReferences) {
        auto refer = FuzzReference::parse(vaildCase.first);
        ASSERT_EQ(refer.has_value(), true)
          << vaildCase.first.toStdString() << " is vaild fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
        ASSERT_EQ(refer->toString().toStdString(),vaildCase.second.toStdString());
    }

    QStringList invaildFuzzReferences = {
        "com.example.App/1.0.0.0/x86_4/runtime",
        "com.example.App/unknown/x86_64/run",
        "com.example.App/1.0..0/unknown/runtime",
        "com.example.App/1.0.0.0/x864/unknown",
        "com.example.App/1.0.0.0/x86_64//unknown",
        "com.example.App/1.0.0.0//",
        "com.example.App/",
        "com.example.App/1.0.0.0/x86_64/dev",
        "/1.0.0.0/x86_64/dev",
    };

    for (const auto &vaildCase : invaildFuzzReferences) {
        auto refer = FuzzReference::parse(vaildCase);
        ASSERT_EQ(refer.has_value(), false)
          << vaildCase.toStdString() << " is invaild fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
    }
}
