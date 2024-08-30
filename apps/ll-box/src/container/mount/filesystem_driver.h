/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "util/util.h"

namespace linglong {

class FilesystemDriver
{
public:
    virtual int Setup() = 0;
    virtual ~FilesystemDriver();
    virtual int CreateDestinationPath(const util::fs::path &container_destination_path) = 0;
    virtual util::fs::path HostPath(const util::fs::path &container_destination_path) const = 0;
    virtual util::fs::path HostSource(const util::fs::path &container_destination_path) const = 0;
};

class OverlayfsFuseFilesystemDriver : public FilesystemDriver
{
public:
    explicit OverlayfsFuseFilesystemDriver(util::str_vec lower_dirs,
                                           std::string upper_dir,
                                           std::string work_dir,
                                           std::string mount_point);

    int Setup() override;

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    util::str_vec lower_dirs_;
    std::string upper_dir_;
    std::string work_dir_;
    std::string mount_point_;
};

class FuseProxyFilesystemDriver : public FilesystemDriver
{
public:
    explicit FuseProxyFilesystemDriver(util::str_vec mounts, std::string mount_point);

    int Setup() override;

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    util::str_vec mounts_;
    std::string mount_point_;
};

class NativeFilesystemDriver : public FilesystemDriver
{
public:
    explicit NativeFilesystemDriver(std::string root_path);
    ~NativeFilesystemDriver();

    int Setup() override { return 0; }

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    std::string root_path_;
};

} // namespace linglong
