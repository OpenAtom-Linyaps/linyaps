/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

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

    int MountNode(const Mount &m) const;

    void finalizeMounts() const;

private:
    std::unique_ptr<HostMountPrivate> dd_ptr;
};

} // namespace linglong
