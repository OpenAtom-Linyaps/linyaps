/*
 * SPDX-FileCopyrightText: 2015 Microsoft Corporation.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Code in this file comes from
// https://github.com/microsoft/GSL/blob/52212c2d7600c816ec9b0438d3fbd2a95c190c2f/include/gsl/util

///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#include <utility>

namespace linglong::utils::finally {

// index type for all container indexes/subscripts/sizes
using index = std::ptrdiff_t;

// final_action allows you to ensure something gets run at the end of a scope
template <class F>
class final_action
{
public:
    explicit final_action(const F &ff) noexcept
        : f{ ff }
    {
    }

    explicit final_action(F &&ff) noexcept
        : f{ std::move(ff) }
    {
    }

    ~final_action() noexcept
    {
        if (invoke)
            f();
    }

    final_action(final_action &&other) noexcept
        : f(std::move(other.f))
        , invoke(std::exchange(other.invoke, false))
    {
    }

    final_action(const final_action &) = delete;
    void operator=(const final_action &) = delete;
    void operator=(final_action &&) = delete;

private:
    F f;
    bool invoke = true;
};

// finally() - convenience function to generate a final_action
template <class F>
[[nodiscard]] auto finally(F &&f) noexcept
{
    return final_action<std::decay_t<F>>{ std::forward<F>(f) };
}
} // namespace linglong::utils::finally
