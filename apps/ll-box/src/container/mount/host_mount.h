/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_
#define LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_

#include "util/oci_runtime.h"

namespace linglong {

class FilesystemDriver;
class HostMountPrivate;

class HostMount
{
public:
    HostMount();
    ~HostMount();

    int Setup(FilesystemDriver *driver);

    int MountNode(const Mount &m);

private:
    std::unique_ptr<HostMountPrivate> dd_ptr;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_ */
