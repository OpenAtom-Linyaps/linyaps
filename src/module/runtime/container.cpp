/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container.h"

#include "module/util/file.h"
#include "module/util/uuid.h"
#include "module/util/xdg.h"

linglong::util::Error Container::create(const QString &ref)
{
    auto containerID = linglong::util::genUuid();
    auto containerWorkDirectory = linglong::util::userRuntimeDir().absoluteFilePath(
            QString("linglong/%1").arg(containerID));

    id = containerID;
    workingDirectory = containerWorkDirectory;
    linglong::util::ensureDir(workingDirectory);

    packageName = ref;

    return NoError();
}
