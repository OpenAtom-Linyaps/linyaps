// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/dir.h"

#include "linglong/common/xdg.h"

namespace linglong::common::dir {

std::filesystem::path getRuntimeDir() noexcept
{
    return xdg::getXDGRuntimeDir() / "linglong";
}

std::filesystem::path getAppRuntimeDir(const std::string &appId) noexcept
{
    return getRuntimeDir() / "apps" / appId;
}

std::filesystem::path getBundleDir(const std::string &containerId) noexcept
{
    return getRuntimeDir() / containerId;
}

} // namespace linglong::common::dir
