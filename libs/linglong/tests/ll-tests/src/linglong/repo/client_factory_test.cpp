/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/repo/client_factory.h"

#include <QString>

using namespace linglong::repo;

class ClientFactoryTest : public ::testing::Test
{
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(ClientFactoryTest, CreateWithQString)
{
    ClientFactory factory(QString("http://localhost:8081"));
    auto client = factory.createClientV2();
    EXPECT_NE(client, nullptr);
}

TEST_F(ClientFactoryTest, CreateWithStdString)
{
    ClientFactory factory(std::string("http://localhost:8081"));
    auto client = factory.createClientV2();
    EXPECT_NE(client, nullptr);
}

TEST_F(ClientFactoryTest, SetServerWithQString)
{
    ClientFactory factory(QString("http://localhost:8080"));
    factory.setServer(QString("http://localhost:8084"));
    auto client = factory.createClientV2();
    EXPECT_NE(client, nullptr);
}

TEST_F(ClientFactoryTest, SetServerWithStdString)
{
    ClientFactory factory(std::string("http://localhost:8080"));
    factory.setServer(std::string("http://localhost:8083"));
    auto client = factory.createClientV2();
    EXPECT_NE(client, nullptr);
}
