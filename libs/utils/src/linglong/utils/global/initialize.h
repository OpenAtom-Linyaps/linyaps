/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

namespace linglong::utils::global {

void applicationInitializte(bool appForceStderrLogging = false);
void installMessageHandler();
bool linglongInstalled();

} // namespace linglong::utils::global
