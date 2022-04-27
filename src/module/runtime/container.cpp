/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "container.h"

#include "module/util/uuid.h"
#include "module/util/xdg.h"
#include "module/util/fs.h"


linglong::util::Error Container::create(const QString& ref)
{
    auto containerID = linglong::util::genUuid();
    auto containerWorkDirectory = linglong::util::userRuntimeDir().absoluteFilePath(QString("linglong/%1").arg(containerID));

    id = containerID;
    workingDirectory = containerWorkDirectory;
    linglong::util::ensureDir(workingDirectory);

    packageName = ref;

    return NoError();
}
