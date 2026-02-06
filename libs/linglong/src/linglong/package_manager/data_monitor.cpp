// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "data_monitor.h"

#include "linglong/utils/log/log.h"

#include <fmt/format.h>

namespace linglong::service {

DataMonitor::DataMonitor(int range, int interval, std::function<void(DataMonitor &)> cb)
    : range(range)
    , interval(interval)
    , total(0)
    , statistics(range, 0)
    , currentIndex(0)
    , actualRange(0)
    , cb(std::move(cb))
    , running(false)
    , paused(false)
    , currentSpeed(0.0)
{
}

DataMonitor::~DataMonitor()
{
    stop();
}

void DataMonitor::dataArrived(uint64_t arrived)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    total += arrived;
}

void DataMonitor::start()
{
    if (running.load()) {
        return;
    }

    running.store(true);
    monitorThread = std::thread(&DataMonitor::monitorLoop, this);
}

void DataMonitor::stop()
{
    if (!running.load()) {
        return;
    }

    running.store(false);
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}

void DataMonitor::pause(bool pause)
{
    paused.store(pause);
}

void DataMonitor::monitorLoop()
{
    while (running.load()) {
        {
            std::lock_guard<std::mutex> lock(dataMutex);

            statistics[currentIndex] = total;
            currentIndex = (currentIndex + 1) % range;

            uint64_t totalDataInRange = 0;
            for (const auto &data : statistics) {
                totalDataInRange += data;
            }

            actualRange = std::min(range, actualRange + 1);
            if (actualRange > 0) {
                currentSpeed.store(static_cast<double>(totalDataInRange) / actualRange);
            }

            total = 0;
        }

        if (!paused.load() && cb) {
            cb(*this);
        }

        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
}

double DataMonitor::getCurrentSpeed()
{
    return currentSpeed.load();
}

std::string DataMonitor::getHumanSpeed()
{
    const char *units[] = { "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };
    double speed = getCurrentSpeed();
    int unitIndex = 0;

    while (speed >= 1024.0 && unitIndex < 4) {
        speed /= 1024.0;
        unitIndex++;
    }

    if (unitIndex == 0) {
        return fmt::format("{:.0f}{}", speed, units[unitIndex]);
    } else {
        return fmt::format("{:.2f}{}", speed, units[unitIndex]);
    }
}

} // namespace linglong::service
