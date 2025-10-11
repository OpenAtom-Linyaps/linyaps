// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>

namespace linglong::common::dir {

// linglong Runtime directory is $XDG_RUNTIME_DIR/linglong, for example:
// /run/user/$UID/linglong
// /tmp/linglong-runtime-$UID/linglong
std::filesystem::path getRuntimeDir() noexcept;

std::filesystem::path getAppRuntimeDir(const std::string &appId) noexcept;

std::filesystem::path getBundleDir(const std::string &containerId) noexcept;

} // namespace linglong::common::dir
