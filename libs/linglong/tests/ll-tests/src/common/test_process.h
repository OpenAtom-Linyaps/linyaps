// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "common/tempdir.h"

#include <chrono>
#include <string>
#include <vector>

#include <sys/types.h>

namespace linglong::test {

// Launch the same binary in helper mode. Each child has its own process group,
// so an early assertion failure also terminates commands started by the helper.
class TestProcess
{
public:
    explicit TestProcess(std::vector<std::string> arguments);
    ~TestProcess();

    TestProcess(const TestProcess &) = delete;
    TestProcess &operator=(const TestProcess &) = delete;

    int wait(std::chrono::milliseconds timeout = std::chrono::seconds(10));
    std::string stderrOutput() const;

private:
    TempDir outputDirectory;
    pid_t pid{ -1 };
    int exitCode{ -1 };
};

int runTestHelper(const std::vector<std::string> &arguments);
int runFileLockHelper(const std::vector<std::string> &arguments);
int runBuilderHelper(const std::vector<std::string> &arguments);
int runSourceFetchHelper(const std::vector<std::string> &arguments);

} // namespace linglong::test
