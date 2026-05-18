// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <new>
#include <optional>
#include <vector>

#include <sys/uio.h>

namespace linglong::utils::io {

#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t hardware_constructive_interference_size =
  std::hardware_constructive_interference_size;
#else
inline constexpr std::size_t hardware_constructive_interference_size = 64;
#endif

class alignas(hardware_constructive_interference_size) RingBuffer
{
public:
    static std::optional<RingBuffer> create(std::size_t requestedCapacity) noexcept
    {
        auto cap = roundUpPowerOf2(requestedCapacity);
        if (cap == 0) {
            return std::nullopt;
        }

        std::vector<std::byte> data(cap);
        return RingBuffer{ std::move(data), cap - 1 };
    }

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;
    RingBuffer(RingBuffer &&) = default;
    RingBuffer &operator=(RingBuffer &&) = default;
    ~RingBuffer() = default;

    [[nodiscard]] std::array<struct iovec, 2> writeIovecs() noexcept
    {
        auto writePos = head_ & mask_;
        auto contiguous = std::min(freeSpace(), capacity() - writePos);

        std::array<struct iovec, 2> iov{};
        iov[0].iov_base = data_.data() + writePos;
        iov[0].iov_len = contiguous;

        auto remaining = freeSpace() - contiguous;
        if (remaining > 0) {
            iov[1].iov_base = data_.data();
            iov[1].iov_len = remaining;
        }

        return iov;
    }

    [[nodiscard]] std::array<struct iovec, 2> readIovecs() const noexcept
    {
        auto readPos = tail_ & mask_;
        auto contiguous = std::min(size(), capacity() - readPos);

        std::array<struct iovec, 2> iov{};
        iov[0].iov_base = const_cast<std::byte *>(data_.data() + readPos);
        iov[0].iov_len = contiguous;

        auto remaining = size() - contiguous;
        if (remaining > 0) {
            iov[1].iov_base = const_cast<std::byte *>(data_.data());
            iov[1].iov_len = remaining;
        }

        return iov;
    }

    void commitWrite(std::size_t n) noexcept { head_ += n; }
    void commitRead(std::size_t n) noexcept { tail_ += n; }

    [[nodiscard]] std::size_t size() const noexcept { return head_ - tail_; }
    [[nodiscard]] std::size_t freeSpace() const noexcept { return (mask_ + 1) - (head_ - tail_); }
    [[nodiscard]] bool empty() const noexcept { return head_ == tail_; }
    [[nodiscard]] bool full() const noexcept { return (head_ - tail_) == (mask_ + 1); }
    [[nodiscard]] std::size_t capacity() const noexcept { return mask_ + 1; }
    void clear() noexcept
    {
        head_ = 0;
        tail_ = 0;
    }

private:
    RingBuffer(std::vector<std::byte> data, std::size_t mask) noexcept
        : data_(std::move(data))
        , mask_(mask)
    {
    }

    static std::size_t roundUpPowerOf2(std::size_t v) noexcept
    {
        if (v == 0) {
            return 0;
        }
        v--;
        v |= v >> 1U;
        v |= v >> 2U;
        v |= v >> 4U;
        v |= v >> 8U;
        v |= v >> 16U;
        if constexpr (sizeof(std::size_t) > 4) {
            v |= v >> 32U;
        }
        return v + 1;
    }

    std::vector<std::byte> data_;
    std::size_t head_{ 0 };
    std::size_t tail_{ 0 };
    std::size_t mask_;
};

} // namespace linglong::utils::io
