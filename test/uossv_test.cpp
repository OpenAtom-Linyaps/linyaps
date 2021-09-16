/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "module/package/package.h"

#include <QJsonDocument>
#include <QFile>

#include <libuossv.h>

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
    GoString certPath = {
        .p = priv_crt.c_str(),
        .n = priv_crt.length()

    };
    GoString key = {
        .p = priv_key.c_str(),
        .n = priv_key.length()

    };
    GoUint8 noTSA = 0;
    GoUint8 timeout = 0;
    GoUint8 dump = 1;
    //GoSlice *sd = (GoSlice *)malloc(sizeof(GoInt) * 2 + sizeof(char) * 1024);
    GoSlice output ;
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
