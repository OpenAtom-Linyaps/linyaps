// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"

#include <memory>
#include <string>

namespace linglong::ctk::detect {

class ApplicationSingleton final
{
public:
    explicit ApplicationSingleton(const std::string &lockFilePath);
    ~ApplicationSingleton();

    utils::error::Result<bool> tryAcquireLock();
    void releaseLock();

    bool isLockHeld() const { return lockHeld_; }

    ApplicationSingleton(const ApplicationSingleton &) = delete;
    ApplicationSingleton &operator=(const ApplicationSingleton &) = delete;

private:
    std::string lockFilePath_;
    std::unique_ptr<linglong::utils::filelock::FileLock> fileLock_;
    bool lockHeld_ = false;
};

} // namespace linglong::ctk::detect
