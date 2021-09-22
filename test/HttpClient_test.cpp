/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:
 *
 * Maintainer:
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

#include "module/util/HttpClient.h"

TEST(HttpClientT01, loadHttpData)
{
    // test invalid dir
    const QString savePath = "/home/xxxx/8.linglong/test3";
    const QString url = "https://cdn.zoom.us/prod/5.7.29123.0808/zoom_x86_64.tar.xz";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    EXPECT_EQ(httpClient->loadHttpData(url, savePath), false);
    httpClient->release();
}

TEST(HttpClientT02, loadHttpData)
{
    const QString savePath = "/home/xxxx/8.linglong/test";
    //test invalid http
    const QString url = "httpsa://cdn.zoom.us/prod/5.7.29123.0808/zoom_x86_64.tar.xz";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    EXPECT_EQ(httpClient->loadHttpData(url, savePath), false);
    httpClient->release();
}

TEST(HttpClientT03, loadHttpData)
{
    const QString savePath = NULL;
    const QString url = "https://cdn.zoom.us/prod/5.7.29123.0808/zoom_x86_64.tar.xz";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    EXPECT_EQ(httpClient->loadHttpData(url, savePath), false);
    httpClient->release();
}

TEST(HttpClientT04, loadHttpData)
{
    const QString savePath = "";
    const QString url = "http://10.20.52.184/hqhdebstore/pool/main/l/llcmd/llcmd_1.0.0-1_amd64.deb";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    EXPECT_EQ(httpClient->loadHttpData(url, savePath), false);
    httpClient->release();
}

// TEST(HttpClientT05, loadHttpData)
// {
//     const QString savePath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/test";
//     const QString url = "http://10.20.52.184/hqhdebstore/pool/main/l/llcmd/llcmd_1.0.0-1_amd64.deb";
//     linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
//     EXPECT_EQ(httpClient->loadHttpData(url, savePath), true);
//     httpClient->release();
// }