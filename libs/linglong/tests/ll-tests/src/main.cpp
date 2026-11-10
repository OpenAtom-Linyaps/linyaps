/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "common/test_process.h"
#include "linglong/common/global/initialize.h"

#include <QByteArray>

#include <string_view>

int main(int argc, char **argv)
{
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Console);
    if (argc > 1 && std::string_view(argv[1]) == "--test-helper") {
        const auto result = linglong::test::runTestHelper({ argv + 2, argv + argc });
        // Builder mocks may report setup failures through GoogleTest assertions.
        return result != 0 ? result : (testing::Test::HasFailure() ? 1 : 0);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
