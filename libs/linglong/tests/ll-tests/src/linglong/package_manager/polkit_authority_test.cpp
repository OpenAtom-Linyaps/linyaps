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
        const auto subject = selectPolkitSubject(action, ":1.42", "c3", true);
        EXPECT_EQ(subject.kind, "unix-session");
        EXPECT_EQ(subject.value, "c3");
    }
}

TEST(PolkitAuthorityTest, SeparateCliProcessesShareTheSameKeepSubject)
{
    constexpr auto action = "org.deepin.linglong.PackageManager1.install";

    // Each ll-cli invocation opens a new bus connection, but both are in the
    // same login session. polkit must see the same subject for both checks.
    const auto first = selectPolkitSubject(action, ":1.42", "c3", true);
    const auto second = selectPolkitSubject(action, ":1.99", "c3", true);
    EXPECT_EQ(first.kind, "unix-session");
    EXPECT_EQ(second.kind, first.kind);
    EXPECT_EQ(second.value, first.value);

    // A different login session must not inherit that authorization.
    const auto otherSession = selectPolkitSubject(action, ":1.100", "c4", true);
    EXPECT_EQ(otherSession.kind, "unix-session");
    EXPECT_NE(otherSession.value, first.value);
}

TEST(PolkitAuthorityTest, PlainAdminActionUsesCallerBusName)
{
    for (const auto *action : {
           "org.deepin.linglong.PackageManager1.prune",
           "org.deepin.linglong.PackageManager1.set-configuration",
         }) {
        const auto subject = selectPolkitSubject(action, ":1.42", "c3", true);
        EXPECT_EQ(subject.kind, "system-bus-name");
        EXPECT_EQ(subject.value, ":1.42");
    }
}

TEST(PolkitAuthorityTest, MissingSessionFallsBackToCallerBusName)
{
    const auto subject = selectPolkitSubject("org.deepin.linglong.PackageManager1.install",
                                             ":1.42",
                                             "",
                                             true);
    EXPECT_EQ(subject.kind, "system-bus-name");
    EXPECT_EQ(subject.value, ":1.42");
}

TEST(PolkitAuthorityTest, InactiveSessionUsesCallerBusName)
{
    const auto subject = selectPolkitSubject("org.deepin.linglong.PackageManager1.install",
                                             ":1.42",
                                             "c3",
                                             false);
    EXPECT_EQ(subject.kind, "system-bus-name");
    EXPECT_EQ(subject.value, ":1.42");
}

} // namespace
