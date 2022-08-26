/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_
#define LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_

#include <QObject>
#include <QDBusContext>

namespace linglong {
namespace system {
namespace helper {

class SystemHelper
    : public QObject
    , protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.SystemHelper")
public:
    const char *ServiceName = "org.deepin.linglong.SystemHelper";
    const char *ServicePath = "/org/deepin/linglong/SystemHelper";
};

} // namespace helper
} // namespace system
} // namespace linglong

#endif // LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_