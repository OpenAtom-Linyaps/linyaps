/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:
 *
 * Maintainer:
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

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <thread>
#include "service/util/singleton.h"

using std::cout;
using std::endl;
using std::vector;

using linglong::service::util::AppInstance;
using linglong::service::util::AppInfo;

std::vector<std::thread> thread_list;

std::mutex mtx_thread_app_list;
std::mutex mtx_thread_list;
vector<decltype(AppInstance::get())> thread_app_list;

/*!
 * thread loop
 */
void loop_thread()
{
    std::unique_lock<std::mutex> lock_thread(mtx_thread_app_list, std::try_to_lock);
    if (lock_thread.owns_lock()) {
        auto app = AppInstance::get();

        thread_app_list.push_back(app);

        std::cout << "tid:" << std::this_thread::get_id() << "\to: " << app << std::endl;

        //        std::this_thread::sleep_for(std::chrono::seconds(value));
    }
    lock_thread.unlock();
};

TEST(Singleton, singleinit_01)
{
    auto app = AppInstance::get();
    auto app2 = linglong::service::util::AppInstance::get();

    // check singleton
    EXPECT_EQ(app, app2);

    AppInfo app_info;
    app_info.appid = "org.test.app1";
    app_info.version = "v0.1";
    app->AppendAppInstance<AppInfo>(app_info);

    // check appinfo
    std::unique_lock<std::mutex> lock(app->mtx_lock);
    for (const auto &it : app->app_ump) {
        for (const auto &app : it.second) {
            cout << "appid: " << app.appid << "\tversion: " << app.version << endl;
            EXPECT_EQ(app_info.appid, app.appid);
            EXPECT_EQ(app_info.version, app.version);
        }
    }

    EXPECT_EQ(&app->app_ump, &app2->app_ump);

    lock.unlock();

    // push thread
    for (int i = 0; i < 10; i++) {
        thread_list.emplace_back(loop_thread);
    }

    // join thread
    for (auto &job : thread_list) {
        if (job.joinable())
            job.join();
    }

    // check result
    std::unique_lock<std::mutex> lock2(app->mtx_lock);
    for (auto &app_new : thread_app_list) {
        EXPECT_EQ(app, app_new);
        EXPECT_EQ(&app->app_ump, &app_new->app_ump);
    }
    lock2.unlock();
}
