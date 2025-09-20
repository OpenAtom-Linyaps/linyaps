// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>

namespace linglong::utils {

class OverlayFS
{
public:
    OverlayFS() = delete;
    OverlayFS(const OverlayFS &) = delete;
    OverlayFS &operator=(const OverlayFS &) = delete;

    OverlayFS(std::filesystem::path lowerdir,
              std::filesystem::path upperdir,
              std::filesystem::path workdir,
              std::filesystem::path merged);
    ~OverlayFS();

    bool mount();
    void unmount(bool clean = false);

    std::filesystem::path upperDirPath() { return upperdir_; }

    std::filesystem::path mergedDirPath() { return merged_; }

private:
    std::filesystem::path lowerdir_;
    std::filesystem::path upperdir_;
    std::filesystem::path workdir_;
    std::filesystem::path merged_;
};

} // namespace linglong::utils
