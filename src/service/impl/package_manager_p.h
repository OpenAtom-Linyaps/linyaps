/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang <yuanqiliang@uniontech.com>
 *
 * Maintainer: yuanqiliang <yuanqiliang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "module/runtime/app.h"
#include "module/repo/repo.h"
#include "module/repo/ostree_repo.h"
#include "package_manager/impl/app_status.h"
#include "module/util/sysinfo.h"

namespace linglong {
namespace service {
class PackageManager;
class PackageManagerPrivate
    : public QObject
{
    Q_OBJECT
public:
    explicit PackageManagerPrivate(PackageManager *parent);
    ~PackageManagerPrivate() override = default;

private:
    QMap<QString, QPointer<linglong::runtime::App>> apps;
    linglong::repo::OSTreeRepo repo;

    /**
     * @brief 查询应用是否正在运行
     *
     * @param appId: 应用的appId
     * @param version: 应用的版本
     * @param arch: 应用的架构
     *
     * @return bool: true 正在运行 false 未运行
     */
    bool isAppRunning(const QString &appId, const QString &version, const QString &arch);

public:
    PackageManager *const q_ptr;
    Q_DECLARE_PUBLIC(PackageManager);
};
} // namespace service
} // namespace linglong
