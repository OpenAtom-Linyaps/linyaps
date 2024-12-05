// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "sha256.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <random>

TEST(sha256, same_as_openssl)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(static_cast<int>(std::numeric_limits<std::byte>::min()),
                                         static_cast<int>(std::numeric_limits<std::byte>::max()));

    std::array<std::byte, 16384> data{};
    std::generate(data.begin(), data.end(), [&gen, &dist]() {
        return static_cast<std::byte>(dist(gen));
    });

    std::array<std::byte, 32> digest1{};
    digest::SHA256 sha256_1;
    sha256_1.update(data.data(), data.size());
    sha256_1.final(digest1.data());

    std::array<std::byte, 32> digest2{};

    unsigned int len{ 0 };
    auto ret = EVP_Digest(data.data(),
                          data.size(),
                          reinterpret_cast<unsigned char *>(digest2.data()),
                          &len,
                          EVP_sha256(),
                          nullptr);
    ASSERT_NE(ret, 0);
    EXPECT_EQ(digest1, digest2);
}
