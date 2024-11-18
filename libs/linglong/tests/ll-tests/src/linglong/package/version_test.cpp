/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/version.h"

using namespace linglong::package;

TEST(Package, VersionRegex101)
{
    // tests modified from https://regex101.com/r/vkijKf/1/

    QStringList validCases = {
        "0.0.0.4",
        "1.2.3.4",
        "10.20.30.40",
        "1.0.0.0",
        "2.0.0.0",
        "1.1.1.7",
        "999999999999999999.999999999999999999.99999999999999999.99999999999999999",
    };

    for (const auto &validCase : validCases) {
        auto res = Version::parse(validCase);
        ASSERT_EQ(res.has_value(), true) << validCase.toStdString() << " is valid.";
        ASSERT_EQ(res->toString().toStdString(), validCase.toStdString());
    }

    QStringList invalidCases = {
        "1",
        "1.2",
        "1.2.3-0123",
        "1.2.3.4-0123",
        "1.2.3-0123.0123",
        "1.2.3.4-0123.0123",
        "1.1.2+.123",
        "1.1.1.2+.123",
        "+invalid",
        "-invalid",
        "-invalid+invalid",
        "-invalid.01",
        "alpha",
        "alpha.beta",
        "alpha.beta.1",
        "alpha.1",
        "alpha+beta",
        "alpha_beta",
        "alpha.",
        "alpha..",
        "beta",
        "1.0.0-alpha_beta",
        "1.0.0.0-alpha_beta",
        "-alpha.",
        "1.0.0-......",
        "1.0.0-alpha..",
        "1.0.0.0-alpha..",
        "1.0.0-alpha..1",
        "1.0.0.0-alpha..1",
        "1.0.0-alpha...1",
        "1.0.0.0-alpha...1",
        "1.0.0.0-......",
        "1.0.0-alpha....1",
        "1.0.0.0-alpha....1",
        "1.0.0-alpha.....1",
        "1.0.0.0-alpha.....1",
        "1.0.0-alpha......1",
        "1.0.0.0-alpha......1",
        "1.0.0-alpha.......1",
        "1.0.0.0-alpha.......1",
        "01.1.1",
        "01.1.1.1",
        "1.01.1",
        "1.01.1.1",
        "1.1.01",
        "1.1.01.1",
        "1.1.1.01",
        "1.2",
        "1.2.3.DEV",
        "1.2.3.4.DEV",
        "1.2-SNAPSHOT",
        "1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788",
        "1.2-RC-SNAPSHOT",
        "-1.0.3-gamma+b7718",
        "+justmeta",
        "9.8.7+meta+meta",
        "9.8.7.6+meta+meta",
        "9.8.7-whatever+meta+meta",
        "9.8.7.6-whatever+meta+meta",
        "99999999999999999999999.999999999999999999.99999999999999999.99999999999999999",
        "999999999999999999.99999999999999999999999.99999999999999999.99999999999999999",
        "999999999999999999.99999999999999999.99999999999999999999999.99999999999999999",
        "999999999999999999.99999999999999999.99999999999999999.99999999999999999999999",
        "999999999999999999.999999999999999999.99999999999999999----RC-SNAPSHOT.12.09.1-------"
        "999999999999999999.999999999999999999.99999999999999999.999999999999999999----RC-"
        "SNAPSHOT.12.09.1-------"
        "-------------------------..12",
    };
    for (const auto &invalidCase : invalidCases) {
        auto res = Version::parse(invalidCase);
        ASSERT_EQ(res.has_value(), false) << invalidCase.toStdString() << " is not valid.";
    }
}

TEST(Package, VersionCompare)
{
    QStringList versions = { "1.0.0.0", "1.0.0.1", "2.0.0.0", "2.1.0.0", "2.1.1.0", "2.1.1.1" };
    for (int i = 0; i < versions.size() - 1; i++) {
        for (int j = i + 1; j < versions.size(); j++) {
            auto x = Version::parse(versions[i]);
            ASSERT_EQ(x.has_value(), true) << versions[i].toStdString() << " is valid.";
            auto y = Version::parse(versions[j]);
            ASSERT_EQ(y.has_value(), true) << versions[j].toStdString() << " is valid.";

            ASSERT_EQ(*x < *y, true)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*x <= *y, true)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*y < *x, false)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*y <= *x, false)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*x != *y, true)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*y > *x, true)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*x > *y, false)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
            ASSERT_EQ(*x >= *y, false)
              << "x: " << x->toString().toStdString() << " y:" << y->toString().toStdString();
        }
    }
}
