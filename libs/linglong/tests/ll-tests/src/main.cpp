/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/global/initialize.h"

#include <QByteArray>

int main(int argc, char **argv)
{
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Console);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
