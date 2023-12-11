/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/utils/global/initialize.h"

#include <QByteArray>

int main(int argc, char **argv)
{
    qputenv("QT_FORCE_STDERR_LOGGING", QByteArray("1"));
    linglong::utils::global::installMessageHandler();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
