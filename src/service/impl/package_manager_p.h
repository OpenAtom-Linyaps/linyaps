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
#include "module/repo/ostree.h"
#include "module/repo/repohelper_factory.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/httpclient.h"
#include "module/util/package_manager_param.h"
#include "module/util/sysinfo.h"
#include "module/util/runner.h"
#include "module/util/version.h"

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
    linglong::repo::OSTree repo;

public:
    PackageManager *const q_ptr;
    Q_DECLARE_PUBLIC(PackageManager);
};
} // namespace service
} // namespace linglong
