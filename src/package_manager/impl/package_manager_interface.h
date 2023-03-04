/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_INTERFACE_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_INTERFACE_H_

#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/reply.h"

#include <QString>
#include <QStringList>

namespace linglong {
namespace service {
class PackageManagerInterface
{
public:
    virtual ~PackageManagerInterface() = default;
    virtual Reply Download(const DownloadParamOption &downloadParamOption) = 0;
    virtual Reply Install(const InstallParamOption &installParamOption) = 0;
    virtual Reply Uninstall(const UninstallParamOption &paramOption) = 0;
    virtual QueryReply Query(const QueryParamOption &paramOption) = 0;
    virtual Reply Update(const ParamOption &paramOption) = 0;
    virtual Reply Exec(const ExecParamOption &paramOption) = 0;
};

} // namespace service
} // namespace linglong
#endif
