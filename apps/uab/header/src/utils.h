// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <utility>

template <typename Func>
struct defer
{
    defer(defer &&newF) = delete;
    defer(const defer &newF) = delete;
    defer() = delete;
    defer &operator=(const defer &) = delete;
    defer &operator=(defer &&) = delete;

    explicit defer(Func newF)
        : f(std::move(newF))
    {
    }

    ~defer() { f(); }

private:
    Func f;
};
