/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_FLATPAK_FLATPAK_MANAGER_H_
#define LINGLONG_SRC_MODULE_FLATPAK_FLATPAK_MANAGER_H_

#include <QStringList>

#include "module/util/singleton.h"

namespace linglong {
namespace flatpak {

class FlatpakManager : public linglong::util::Singleton<FlatpakManager>
{
public:
    QString getAppPath(const QString &appId);

    QString getRuntimePath(const QString &appId);

    QStringList getAppDesktopFileList(const QString &appId);

    friend class linglong::util::Singleton<FlatpakManager>;
};

} // namespace flatpak
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_FLATPAK_FLATPAK_MANAGER_H_
