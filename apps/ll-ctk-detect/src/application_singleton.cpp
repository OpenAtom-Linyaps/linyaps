// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "application_singleton.h"

#include "linglong/utils/log/log.h"

#include <filesystem>

namespace linglong::ctk::detect {

ApplicationSingleton::ApplicationSingleton(const std::string &lockFilePath)
    : lockFilePath_(lockFilePath)
{
}

ApplicationSingleton::~ApplicationSingleton()
{
    releaseLock();
}

utils::error::Result<bool> ApplicationSingleton::tryAcquireLock()
{
    LINGLONG_TRACE("try acquire singleton lock")

    if (lockHeld_) {
        return true;
    }

    std::filesystem::path lockPath(lockFilePath_);
    std::filesystem::path lockDir = lockPath.parent_path();

    if (!lockDir.empty() && !std::filesystem::exists(lockDir)) {
        std::error_code ec;
        std::filesystem::create_directories(lockDir, ec);
        if (ec) {
            return LINGLONG_ERR("failed to create lock directory: " + ec.message());
        }
    }

    auto fileLock =
      linglong::utils::filelock::FileLock::create(lockFilePath_,
                                                  linglong::utils::filelock::LockType::Write,
                                                  true);
    if (!fileLock) {
        return LINGLONG_ERR(fileLock);
    }

    auto lockResult = fileLock->tryLock(linglong::utils::filelock::LockType::Write);
    if (!lockResult) {
        return LINGLONG_ERR(lockResult);
    }

    if (*lockResult) {
        fileLock_ = std::make_unique<linglong::utils::filelock::FileLock>(std::move(*fileLock));
        lockHeld_ = true;
        return true;
    }

    return false;
}

void ApplicationSingleton::releaseLock()
{
    if (lockHeld_ && fileLock_) {
        auto result = fileLock_->unlock();
        if (!result) {
            LogF("failed to release file lock: {}", result.error().message());
        }
        fileLock_.reset();
        lockHeld_ = false;
    }
}

} // namespace linglong::ctk::detect
