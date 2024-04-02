// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/transaction.h"

void test(int &initNum, bool success)
{

    linglong::utils::Transaction t;

    initNum += 5;
    t.addRollBack(
      [](int &num) noexcept {
          num -= 5;
      },
      std::ref(initNum));

    initNum *= 2;
    t.addRollBack([&initNum]() noexcept {
        initNum /= 2;
    });

    if (success) {
        t.commit();
        return;
    }
}

TEST(Transaction, Success)
{
    int num{ 0 };
    test(num, true);
    EXPECT_EQ(num, 10);
}

TEST(Transaction, Failed)
{
    int num{ 0 };
    test(num, false);
    EXPECT_EQ(num, 0);
}
