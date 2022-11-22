/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_SINGLETON_H_
#define LINGLONG_SRC_MODULE_UTIL_SINGLETON_H_

namespace linglong {
namespace util {

template<typename T>
class Singleton
{
public:
    static T *instance()
    {
        static T instance;
        return &instance;
    }

    Singleton(T &&) = delete;
    Singleton(const T &) = delete;
    void operator=(const T &) = delete;

protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

} // namespace util
} // namespace linglong
#endif