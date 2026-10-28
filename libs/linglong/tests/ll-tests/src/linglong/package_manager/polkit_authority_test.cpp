// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/package_manager/polkit_authority.h"

namespace {

using linglong::service::selectPolkitSubject;

TEST(PolkitAuthorityTest, KeepActionsUseLoginSession)
{
    for (const auto *action : {
           "org.deepin.linglong.PackageManager1.install",
           "org.deepin.linglong.PackageManager1.install-from-file",
           "org.deepin.linglong.PackageManager1.uninstall",
           "org.deepin.linglong.PackageManager1.update",
         }) {
        const auto subject = selectPolkitSubject(action, ":1.42", "c3");
        EXPECT_EQ(subject.kind, "unix-session");
        EXPECT_EQ(subject.value, "c3");
    }
}

TEST(PolkitAuthorityTest, PlainAdminActionUsesCallerBusName)
{
    const auto subject = selectPolkitSubject("org.deepin.linglong.PackageManager1.prune",
                                             ":1.42",
                                             "c3");
    EXPECT_EQ(subject.kind, "system-bus-name");
    EXPECT_EQ(subject.value, ":1.42");
}

TEST(PolkitAuthorityTest, MissingSessionFallsBackToCallerBusName)
{
    const auto subject = selectPolkitSubject("org.deepin.linglong.PackageManager1.install",
                                             ":1.42",
                                             "");
    EXPECT_EQ(subject.kind, "system-bus-name");
    EXPECT_EQ(subject.value, ":1.42");
}

} // namespace
