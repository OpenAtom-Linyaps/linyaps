// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "application_singleton.h"

#include "linglong/utils/log/log.h"

#include <filesystem>

namespace linglong::driver::detect {

ApplicationSingleton::ApplicationSingleton(const std::string &lockFilePath)
    : lockFilePath_(lockFilePath)
    , lockHeld_(false)
{
}

ApplicationSingleton::~ApplicationSingleton()
{
    releaseLock();
}

utils::error::Result<bool> ApplicationSingleton::tryAcquireLock()
{
    LINGLONG_TRACE("Try acquire lock")

    if (lockHeld_) {
        return true;
    }

    // Create directory if it doesn't exist
    std::filesystem::path lockPath(lockFilePath_);
    std::filesystem::path lockDir = lockPath.parent_path();

    if (!lockDir.empty() && !std::filesystem::exists(lockDir)) {
        std::error_code ec;
        std::filesystem::create_directories(lockDir, ec);
        if (ec) {
            return LINGLONG_ERR("Failed to create lock directory: " + ec.message());
        }
    }

    // Try to create file lock
    auto fileLock = linglong::utils::filelock::FileLock::create(lockFilePath_, true);
    if (!fileLock) {
        return LINGLONG_ERR(fileLock);
    }

    // Try to acquire write lock (exclusive)
    auto lockResult = fileLock->tryLock(linglong::utils::filelock::LockType::Write);
    if (!lockResult) {
        return LINGLONG_ERR(lockResult);
    }

    if (*lockResult) {
        fileLock_ = std::make_unique<linglong::utils::filelock::FileLock>(std::move(*fileLock));
        lockHeld_ = true;
        return true;
    }

    // Another instance is already running
    return false;
}

void ApplicationSingleton::releaseLock()
{
    if (lockHeld_ && fileLock_) {
        auto result = fileLock_->unlock();
        if (!result) {
            // Log error but don't throw - we're in destructor
            LogF("Failed to release file lock: {}", result.error().message());
        }
        fileLock_.reset();
        lockHeld_ = false;
    }
}

} // namespace linglong::driver::detect
