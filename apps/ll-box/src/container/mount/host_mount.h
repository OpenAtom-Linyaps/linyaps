/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_
#define LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_

#include "util/oci_runtime.h"

#include <filesystem>

namespace linglong {

struct remountNode
{
    uint32_t flags{ 0U };
    uint32_t extensionFlags{ 0U };
    int targetFd{ -1 };
    std::string data;
};

class HostMount
{
public:
    explicit HostMount(std::filesystem::path containerRoot);
    ~HostMount() = default;

    [[nodiscard]] bool MountNode(const Mount &m);
    static bool remount(const std::filesystem::path &target,
                        uint32_t flags,
                        const std::string &data);
    void finalizeMounts() const;

private:
    std::filesystem::path containerRoot;
    bool sysfs_is_binded{ false };
    std::filesystem::path
    toHostDestination(const std::filesystem::path &containerDestination) noexcept;
    static bool ensureDirectoryExist(const std::filesystem::path &destination) noexcept;
    static bool ensureFileExist(const std::filesystem::path &destination) noexcept;
    static std::optional<bool> isDummy(const std::string& filesystemType) noexcept;
    std::vector<remountNode> remountList;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_ */
