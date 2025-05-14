// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

/*
 * USAGE:
 *
 * Transaction t;
 * t.addRollback([] {...}); // Error: rollback function isn't exception safe
 * t.addRollback([] () { return 1; }); // Error: rollback function may not return anything.
 * t.addRollback([] () noexcept {...}); // OK
 *
 * void rollbackFunc(...) noexcept {...}
 * t.addRollback(rollbackFunc,...); // support invoke a function directly
 *
 * struct Functor { void operator()(...) noexcept {...}};
 * Functor f;
 * t.addRollback(f,...);
 *
 * struct A { A(A&&) noexcept = default; A(const A&) noexcept = default; };
 * A a;
 * t.addRollback([](A ca) noexcept {...}, a) // copy a, ca is available when rollback.
 * t.addRollback([](A) noexcept {...}, std::move(a)) // move a to rollback function, couldn't use a
 * under below context.
 *
 * if you want pass a ref to some function which it use to returning a value, you could:
 * void some_func(std::string& retMsg) {
 *    t.addRollback([](std::string& str) noexcept { // revert retMsg }, std::ref(str));
 * }
 *
 * also could:
 * double c;
 * t.addRollback([&c](...) noexcept {...}, ...);
 * t.addRollback([c](...) noexcept {...}, ...);
 * t.addRollback([newC = c](...) noexcept {...}, ...);
 * and so on.
 */

namespace linglong::utils {

namespace _internal {
struct TransactionRollBackBase
{
    TransactionRollBackBase(TransactionRollBackBase &&) = default;
    TransactionRollBackBase(const TransactionRollBackBase &) = default;
    TransactionRollBackBase &operator=(const TransactionRollBackBase &) = default;
    TransactionRollBackBase &operator=(TransactionRollBackBase &&) = default;
    TransactionRollBackBase() = default;
    virtual ~TransactionRollBackBase() = default;
    virtual void operator()() = 0;
};
} // namespace _internal

template <typename F, typename... Args>
class TransactionRollback final : public _internal::TransactionRollBackBase
{
    static_assert(std::is_nothrow_invocable_v<F, Args...>, "rollback function must be noexcept");
    static_assert(std::is_same_v<std::invoke_result_t<F, Args...>, void>,
                  "rollback function may not return anything.");

public:
    explicit TransactionRollback(F func, Args... args)
        : func(func)
        , m_args(args...)
    {
    }

    void operator()() noexcept override
    {
        if constexpr (sizeof...(Args) > 0) {
            func_with_args(std::make_index_sequence<sizeof...(Args)>{});
        } else {
            func();
        }
    }

private:
    template <std::size_t... I>
    void func_with_args(std::index_sequence<I...> /*unused*/)
    {
        func(std::get<I>(m_args)...);
    }

    F func;
    std::tuple<Args...> m_args;
};

class Transaction
{
public:
    Transaction() = default;
    Transaction(const Transaction &) = delete;
    Transaction(Transaction &&) = delete;
    Transaction &operator=(const Transaction &) = delete;
    Transaction &operator=(Transaction &&) = delete;

    ~Transaction()
    {
        std::for_each(m_rollbacks.rbegin(), m_rollbacks.rend(), [](const auto &func) {
            (*func)();
        });
    }

    void commit() noexcept { m_rollbacks.clear(); }

    template <typename Fn, typename... Args>
    void addRollBack(Fn func, Args... args) noexcept
    {
        m_rollbacks.emplace_back(std::make_unique<TransactionRollback<Fn, Args...>>(func, args...));
    }

private:
    std::vector<std::unique_ptr<_internal::TransactionRollBackBase>> m_rollbacks;
};

}; // namespace linglong::utils
