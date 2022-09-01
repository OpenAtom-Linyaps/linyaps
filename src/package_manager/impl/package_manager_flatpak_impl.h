/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QJsonArray>

#include "module/util/singleton.h"
#include "module/util/status_code.h"
#include "module/package/package.h"
#include "package_manager_interface.h"
#include "service/impl/reply.h"
#include "service/impl/param_option.h"

namespace linglong {
namespace service {
class PackageManagerFlatpakImpl
    : public PackageManagerInterface
    , public linglong::util::Singleton<PackageManagerFlatpakImpl>
{
public:
    ~PackageManagerFlatpakImpl() override = default;
    Reply Download(const DownloadParamOption &downloadParamOption) override { return Reply(); }
    Reply Install(const InstallParamOption &installParamOption) override;
    Reply Uninstall(const UninstallParamOption &paramOption) override;
    QueryReply Query(const QueryParamOption &paramOption) override;
};
} // namespace service
} // namespace linglong

#define PACKAGEMANAGER_FLATPAK_IMPL linglong::service::PackageManagerFlatpakImpl::instance()
