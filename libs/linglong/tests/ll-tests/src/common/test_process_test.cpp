// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/test_process.h"

using linglong::test::TestProcess;

TEST(TestProcessTest, ReportsHelperFailureAndAllowsRepeatedWait)
{
    TestProcess child({ "unknown-helper" });
    EXPECT_EQ(child.wait(), 2);
    EXPECT_NE(child.stderrOutput().find("unknown or missing test helper"), std::string::npos);
    EXPECT_EQ(child.wait(), 2);
}

TEST(TestProcessTest, RejectsMissingSourceFetchRequest)
{
    TempDir temporary;
    TestProcess child({ "source-fetch", (temporary.path() / "missing.json").string() });
    EXPECT_EQ(child.wait(), 2);
    EXPECT_NE(child.stderrOutput().find("cannot open source-fetch helper request"),
              std::string::npos);
}
