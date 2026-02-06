// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

namespace linglong::service {

class DataMonitor
{
public:
    DataMonitor(int range, int interval, std::function<void(DataMonitor &)> cb);
    ~DataMonitor();

    void dataArrived(uint64_t arrived);
    void start();
    void stop();
    void pause(bool paused);

    double getCurrentSpeed();
    std::string getHumanSpeed();

private:
    void monitorLoop();

    int range;
    int interval;
    uint64_t total;
    std::vector<uint64_t> statistics;
    int currentIndex;
    int actualRange;
    std::function<void(DataMonitor &)> cb;
    std::atomic<bool> running;
    std::atomic<bool> paused;
    std::atomic<double> currentSpeed;
    std::thread monitorThread;
    std::mutex dataMutex;
};

} // namespace linglong::service
