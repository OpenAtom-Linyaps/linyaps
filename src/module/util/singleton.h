/*
 * Copyright (c) 2020. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    std::string appid;
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
        auto iter = this->app_ump.find(app.appid);
        if (iter == app_ump.end()) {
            std::vector<T> app_list;
            app_list.push_back(std::move(app));
            this->app_ump[app_list.at(0).appid] = app_list;
            lock.unlock();
        } else {
            auto &app_list = this->app_ump[app.appid];
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