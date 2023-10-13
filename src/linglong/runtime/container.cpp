/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container.h"

#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/uuid.h"
#include "linglong/util/xdg.h"

QSERIALIZER_IMPL(Container);

linglong::util::Error Container::create(const QString &ref)
{
    auto containerID = linglong::util::genUuid();
    auto containerWorkDirectory =
      linglong::util::userRuntimeDir().absoluteFilePath(QString("linglong/%1").arg(containerID));

    id = containerID;
    workingDirectory = containerWorkDirectory;
    linglong::util::ensureDir(workingDirectory);

    packageName = ref;

    return Success();
}
