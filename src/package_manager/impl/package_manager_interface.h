/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>

#include "module/dbus_ipc/reply.h"
#include "module/dbus_ipc/param_option.h"
#include "module/package/ref.h"
#include "module/util/result.h"

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
    virtual Reply Exec(const ExecParamOption &paramOption);

    // TODO: move PackageManagerInterface to namespace
    // TODO: need pure virtual
    virtual Reply Update(const ParamOption &paramOption) { return Reply(); }
};

inline Reply PackageManagerInterface::Exec(const ExecParamOption &paramOption)
{
    Reply reply;
    reply.code = STATUS_CODE(kFail);
    reply.message = "Not supported";
    return reply;
}

} // namespace service
} // namespace linglong
