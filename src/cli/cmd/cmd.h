/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/runtime/container.h"

void showContainer(const ContainerList &list, const QString &format);

int namespaceEnter(pid_t pid, const QStringList &args);