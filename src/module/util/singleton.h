/*
 * Copyright (c) 2020. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

namespace linglong {
namespace service {
namespace util {

class AppInfo
{
public:
    std::string appId;
    std::string pid;
    std::string version;
};

template<typename T>
class Single
{
public:
    std::mutex mtx_lock;

public:
    static T *get()
    {
        static T instance;
        return &instance;
    }

    T *operator->() const
    {
        return get();
    }

protected:
    Single() {};
    ~Single() {};

public:
    Single(const Single &) = delete;
    Single &operator=(const Single &single) = delete;
    Single &operator=(const Single &&single) = delete;
};

class AppInstance : public Single<AppInstance>
{
public:
    friend class Single<AppInstance>;
    std::unordered_map<std::string, std::vector<AppInfo>> app_ump;

    template<typename T = AppInfo>
    bool AppendAppInstance(const T &app)
    {
        std::unique_lock<std::mutex> lock(this->mtx_lock);
        auto iter = this->app_ump.find(app.appId);
        if (iter == app_ump.end()) {
            std::vector<T> app_list;
            app_list.push_back(std::move(app));
            this->app_ump[app_list.at(0).appId] = app_list;
            lock.unlock();
        } else {
            auto &app_list = this->app_ump[app.appId];
            app_list.push_back(app);
            lock.unlock();
        }
        return true;
    };

    void Release()
    {
        std::unique_lock<std::mutex> lock(this->mtx_lock);
        lock.unlock();
    }
};

} // namespace util
} // namespace service
} // namespace linglong