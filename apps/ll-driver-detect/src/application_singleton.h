// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"

#include <string>

namespace linglong::driver::detect {

/**
 * @brief Application singleton manager
 *
 * This class ensures only one instance of the driver detection application
 * can run at a time, replacing the configuration-based frequency control.
 */
class ApplicationSingleton
{
public:
    /**
     * @brief Constructor
     * @param lockFilePath Path to the lock file
     */
    explicit ApplicationSingleton(const std::string &lockFilePath);

    /**
     * @brief Destructor - releases the lock
     */
    ~ApplicationSingleton();

    /**
     * @brief Try to acquire the singleton lock
     * @return true if lock acquired successfully, false if another instance is running
     */
    utils::error::Result<bool> tryAcquireLock();

    /**
     * @brief Release the lock
     */
    void releaseLock();

    /**
     * @brief Check if we currently hold the lock
     * @return true if we hold the lock, false otherwise
     */
    bool isLockHeld() const { return lockHeld_; }

    // Delete copy constructor and assignment operator
    ApplicationSingleton(const ApplicationSingleton &) = delete;
    ApplicationSingleton &operator=(const ApplicationSingleton &) = delete;

private:
    std::string lockFilePath_;
    std::unique_ptr<linglong::utils::filelock::FileLock> fileLock_;
    bool lockHeld_;
};

} // namespace linglong::driver::detect
