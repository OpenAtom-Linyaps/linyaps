/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/version.h"
#include "linglong/package/version_range.h"

using namespace linglong::package;

TEST(Package, VersionRange)
{
    QStringList versions = { "1.0.0.0", "1.0.0.1", "2.0.0.0", "2.1.0.0", "2.1.1.0", "2.1.1.1" };
    for (int i = 0; i < versions.size() - 1; i++) {
        for (int j = i + 1; j < versions.size(); j++) {
            for (int k = 0; k < versions.size(); k++) {
                auto begin = Version::parse(versions[i]);
                ASSERT_EQ(begin.has_value(), true) << versions[i].toStdString() << " is valid.";
                auto end = Version::parse(versions[j]);
                ASSERT_EQ(end.has_value(), true) << versions[j].toStdString() << " is valid.";

                auto rawRange = QString("[%1,%2)").arg(begin->toString()).arg(end->toString());
                auto range = VersionRange::parse(rawRange);
                ASSERT_EQ(range.has_value(), true) << rawRange.toStdString() << " is valid.";

                auto ver = Version::parse(versions[k]);
                ASSERT_EQ(ver.has_value(), true) << versions[k].toStdString() << " is valid.";

                if (i <= k && k < j) {
                    ASSERT_EQ(range->contains(*ver), true)
                      << ver->toString().toStdString() << " is in "
                      << range->toString().toStdString();
                } else {
                    ASSERT_EQ(range->contains(*ver), false)
                      << ver->toString().toStdString() << " is not in "
                      << range->toString().toStdString();
                }
            }
        }
    }
}
