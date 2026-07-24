// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>

#include <signal.h>

namespace linglong::utils::signal {

class SignalBlocker
{
public:
    explicit SignalBlocker(std::initializer_list<int> signals) noexcept
    {
        sigemptyset(&blocked_);
        for (auto sig : signals) {
            sigaddset(&blocked_, sig);
        }

        if (::sigprocmask(SIG_BLOCK, &blocked_, &original_) < 0) {
            return;
        }
        valid_ = true;
    }

    ~SignalBlocker() noexcept
    {
        if (valid_) {
            ::sigprocmask(SIG_SETMASK, &original_, nullptr);
        }
    }

    SignalBlocker(const SignalBlocker &) = delete;
    SignalBlocker &operator=(const SignalBlocker &) = delete;
    SignalBlocker(SignalBlocker &&) = delete;
    SignalBlocker &operator=(SignalBlocker &&) = delete;

    [[nodiscard]] bool valid() const noexcept { return valid_; }

    [[nodiscard]] const sigset_t &blocked() const noexcept { return blocked_; }

    void restore() const noexcept
    {
        if (valid_) {
            ::sigprocmask(SIG_SETMASK, &original_, nullptr);
        }
    }

private:
    sigset_t blocked_{};
    sigset_t original_{};
    bool valid_{ false };
};

} // namespace linglong::utils::signal
