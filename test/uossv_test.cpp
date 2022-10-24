/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QFile>
#include <libuossv/uossv.h>

#include "module/package/package.h"

TEST(UOSSign, UosSign)
{
    string input = "abc";
    GoSlice data = {
        .data = (void *)input.c_str(),
        .len = input.length(),
        .cap = input.length(),
    };
    string priv_crt = "/usr/share/ca-certificates/deepin/private/priv.crt";
    string priv_key = "/usr/share/ca-certificates/deepin/private/priv.key";
    GoString certPath = {.p = priv_crt.c_str(), .n = priv_crt.length()

    };
    GoString key = {.p = priv_key.c_str(), .n = priv_key.length()

    };
    GoUint8 noTSA = 0;
    GoUint8 timeout = 0;
    GoUint8 dump = 1;
    // GoSlice *sd = (GoSlice *)malloc(sizeof(GoInt) * 2 + sizeof(char) * 1024);
    GoSlice output;
    int ret = (int)UOSSign(data, certPath, key, noTSA, timeout, &output);
    EXPECT_EQ(ret, 0);
    ret = -1;
    // GoSlice signData = {
    //     .data = sd->data,
    //     .len = sd->len,
    //     .cap = sd->cap,
    // };
    ret = (int)UOSVerify(data, output, dump);
    EXPECT_EQ(ret, 0);
    // free(sd);
}
