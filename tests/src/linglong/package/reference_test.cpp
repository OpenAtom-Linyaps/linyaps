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
        "main:com.example.App/1.0.0.0/x86_64",
        "some_channel:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0-alpha/arm64",
        "main:com.example.App/1.0.0.1-beta/arm64",
        "main:1111/1.0.0.0/x86_64",
        "main:2222/1.0.0.0/x86_64",
        "main:3333/1.0.0.0-alpha/arm64",
        "main:4444/1.0.0.1-beta/arm64",
    };

    for (const auto &vaildCase : vaildReferences) {
        auto refer = Reference::parse(vaildCase);
        ASSERT_EQ(refer.has_value(), true)
          << vaildCase.toStdString() << " is vaild reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
        ASSERT_EQ(refer->toString().toStdString(), vaildCase.toStdString());
    }

    QStringList invaildReferences = {
        "main:com.example.App//1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.-alpha/arm64",
        "main:1111/1.0.0.0/ x86_64",
        "main:2222/1.0.0.0/unknown",
        "main:3333/1.0.0.0-()/arm64",
        ":1.0.0.1-beta/arm64",
        ":com.example.App/1.0.0.0/x86_64",
    };

    for (const auto &invaildCase : invaildReferences) {
        auto refer = Reference::parse(invaildCase);
        ASSERT_EQ(refer.has_value(), false)
          << invaildCase.toStdString() << " is invaild reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
    }

    QList<QPair<QString, QString>> vaildFuzzReferences = {
        { "unknown:com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/unknown/x86_64", "unknown:com.example.App/unknown/x86_64" },
        { "com.example.App/1.0.0.0/unknown", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App/1.0.0.0", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App", "unknown:com.example.App/unknown/unknown" },
        { "com.example.App/1.0.0.1-beta", "unknown:com.example.App/1.0.0.1-beta/unknown" },
        { "3333/1.0.0.0-alpha/arm64", "unknown:3333/1.0.0.0-alpha/arm64" },
        { "4444/1.0.0.1-beta/arm64", "unknown:4444/1.0.0.1-beta/arm64" },
    };

    for (const auto &vaildCase : vaildFuzzReferences) {
        auto refer = FuzzReference::parse(vaildCase.first);
        ASSERT_EQ(refer.has_value(), true)
          << vaildCase.first.toStdString() << " is vaild fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
        ASSERT_EQ(refer->toString().toStdString(), vaildCase.second.toStdString());
    }

    QStringList invaildFuzzReferences = {
        "com.example.App/1.0.0.0/x86_64/runtime",
        "com.example.App/1.0.0.0/x86_4",
        "com.example.App/1.0..0/unknown",
        "com.example.App/1.0.0.0/x864",
        "com.example.App/1.0.0.0/x86_64//",
        "com.example.App/1.0.0.0//",
        // "com.example.App/", // FIXME: This should be invaild.
        ":/1.0.0.0/x86_64",
    };

    for (const auto &vaildCase : invaildFuzzReferences) {
        auto refer = FuzzReference::parse(vaildCase);
        ASSERT_EQ(refer.has_value(), false)
          << vaildCase.toStdString() << " is invaild fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message().toStdString());
    }
}
