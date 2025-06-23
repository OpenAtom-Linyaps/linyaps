// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// refer: https://zh.wikipedia.org/wiki/SHA-2

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace digest {

namespace details {
constexpr static uint32_t rotate_right(uint32_t x, unsigned n) noexcept
{
    return (x >> n) | (x << (32 - n));
}

constexpr static uint32_t to_big_endian(uint32_t val) noexcept
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return x;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8)
            | ((val & 0x000000ff) << 24));
#endif
}

constexpr static uint64_t to_big_endian(uint64_t val) noexcept
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return x;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((val & 0xff00000000000000ull) >> 56) | ((val & 0x00ff000000000000ull) >> 40)
            | ((val & 0x0000ff0000000000ull) >> 24) | ((val & 0x000000ff00000000ull) >> 8)
            | ((val & 0x00000000ff000000ull) << 8) | ((val & 0x0000000000ff0000ull) << 24)
            | ((val & 0x000000000000ff00ull) << 40) | ((val & 0x00000000000000ffull) << 56));
#endif
}

constexpr static uint32_t sum0(uint32_t x) noexcept
{
    return rotate_right(x, 2) ^ rotate_right(x, 13) ^ rotate_right(x, 22);
}

constexpr static uint32_t sum1(uint32_t x) noexcept
{
    return rotate_right(x, 6) ^ rotate_right(x, 11) ^ rotate_right(x, 25);
}

constexpr static uint32_t sigma0(uint32_t x) noexcept
{
    return rotate_right(x, 7) ^ rotate_right(x, 18) ^ (x >> 3);
}

constexpr static uint32_t sigma1(uint32_t x) noexcept
{
    return rotate_right(x, 17) ^ rotate_right(x, 19) ^ (x >> 10);
}

constexpr static uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) noexcept
{
    return (x & y) ^ ((~x) & z);
}

constexpr static uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) noexcept
{
    return (x & y) ^ (x & z) ^ (y & z);
}

} // namespace details

class SHA256
{
    constexpr static auto block_size = 256 / sizeof(uint32_t);

public:
    SHA256() = default;
    SHA256(const SHA256 &) = delete;
    SHA256(SHA256 &&) = delete;
    SHA256 &operator=(const SHA256 &) = delete;
    SHA256 &operator=(SHA256 &&) = delete;
    ~SHA256() = default;

    void update(const std::byte *data, std::size_t len) noexcept
    {
        // if the current block is not completed, consuming input data to fill the rest of block and
        // transforming data block
        if (pos != 0 && pos + len >= block_size) {
            std::copy_n(data, block_size - pos, &m[pos]);
            transform(m.data(), 1);
            total += block_size * 8;
            data += block_size - pos;
            len -= block_size - pos;
            pos = 0;
        }

        // if the input data is more than one block, only transform the complete blocks
        if (len >= block_size) {
            auto blocks = len / block_size;
            auto bytes = blocks * block_size;
            transform(data, blocks);
            data += bytes;
            len -= bytes;
            total += bytes * 8;
        }

        // copy the rest of input data to the current block and wait for more data
        std::copy_n(data, len, &m[pos]);
        pos += len;
    }

    void final(std::byte *digest) noexcept
    {
        // complete the last block
        total += pos * 8;
        m[pos++] = std::byte(0x80);

        // reverse space to fill the length of the last block
        if (pos > block_size - sizeof(uint32_t) * 2) {
            if (pos != block_size) {
                std::fill_n(&m[pos], block_size - pos, std::byte(0));
            }

            transform(m.data(), 1);
            pos = 0;
        }

        std::fill_n(&m[pos], block_size - pos, std::byte(0));
        auto final_len = details::to_big_endian(total);

        std::copy_n(reinterpret_cast<std::byte *>(&final_len),
                    sizeof(uint64_t) / sizeof(std::byte),
                    &m[block_size - 8]);

        transform(m.data(), 1);
        for (std::size_t i = 0; i < 8; ++i) {
            H[i] = details::to_big_endian(H[i]);
        }

        std::copy_n(reinterpret_cast<std::byte *>(H.data()), 32, digest);
    }

private:
    constexpr void transform(const std::byte *data, std::size_t block_num)
    {
        for (std::size_t i = 0; i < block_num; ++i) {
            std::array<uint32_t, 16> M{};
            for (int j = 0; j < 16; ++j) {
                uint32_t tmp = 0;
                std::memcpy(&tmp, &data[i * 64 + j * 4], 4);
                M[j] = details::to_big_endian(tmp);
            }

            std::array<uint32_t, 64> W{};
            for (std::size_t t = 0; t <= 15; ++t) {
                W[t] = M[t];
            }

            for (std::size_t t = 16; t < 64; ++t) {
                W[t] =
                  details::sigma1(W[t - 2]) + W[t - 7] + details::sigma0(W[t - 15]) + W[t - 16];
            }

            auto a = H[0];
            auto b = H[1];
            auto c = H[2];
            auto d = H[3];
            auto e = H[4];
            auto f = H[5];
            auto g = H[6];
            auto h = H[7];

            for (std::size_t t = 0; t < 64; ++t) {
                auto T1 = h + details::sum1(e) + details::Ch(e, f, g) + K[t] + W[t];
                auto T2 = details::sum0(a) + details::Maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            H[0] += a;
            H[1] += b;
            H[2] += c;
            H[3] += d;
            H[4] += e;
            H[5] += f;
            H[6] += g;
            H[7] += h;
        }
    }

    std::size_t pos{ 0 };
    uint64_t total{ 0 };
    static constexpr std::array<uint32_t, 64> K{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2
    };
    std::array<uint32_t, 8> H{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                               0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    std::array<std::byte, 64> m{};
};

} // namespace digest
