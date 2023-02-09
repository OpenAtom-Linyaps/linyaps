/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_FLATPAK_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_FLATPAK_H_

#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/reply.h"
#include "module/package/package.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"
#include "package_manager_interface.h"

#include <QJsonArray>

namespace linglong {
namespace service {
class PackageManagerFlatpakImpl : public PackageManagerInterface,
                                  public linglong::util::Singleton<PackageManagerFlatpakImpl>
{
public:
    ~PackageManagerFlatpakImpl() override = default;

    Reply Download(const DownloadParamOption &downloadParamOption) override { return Reply(); }

    Reply Install(const InstallParamOption &installParamOption) override;
    Reply Uninstall(const UninstallParamOption &paramOption) override;
    QueryReply Query(const QueryParamOption &paramOption) override;

    Reply Update(const ParamOption &paramOption) override { return Reply(); }

    inline Reply Exec(const ExecParamOption &paramOption) override
    {
        Reply reply;
        reply.code = STATUS_CODE(kFail);
        reply.message = "Not supported";
        return reply;
    }
};
} // namespace service
} // namespace linglong

#define PACKAGEMANAGER_FLATPAK_IMPL linglong::service::PackageManagerFlatpakImpl::instance()
#endif