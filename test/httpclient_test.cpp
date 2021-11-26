/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:
 *
 * Maintainer:
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "src/module/util/httpclient.h"

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

TEST(HttpClientT05, loadHttpData)
{
    const QString savePath = "/home/xxxx/8.linglong/GitPrj/debug/linglong/build/test";
    const QString url = "http://10.20.52.184/hqhdebstore/pool/main/l/llcmd/llcmd_1.0.0-1_amd64.deb";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    EXPECT_EQ(httpClient->loadHttpData(url, savePath), false);
    httpClient->release();
}