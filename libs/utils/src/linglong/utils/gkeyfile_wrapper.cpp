/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/gkeyfile_wrapper.h"

namespace linglong::utils {

const GKeyFileWrapper::GroupName GKeyFileWrapper::DesktopEntry = "Desktop Entry";
const GKeyFileWrapper::GroupName GKeyFileWrapper::DBusService = "D-BUS Service";
const GKeyFileWrapper::GroupName GKeyFileWrapper::SystemdService = "Service";
const GKeyFileWrapper::GroupName GKeyFileWrapper::ContextMenu = "Menu Entry";

} // namespace linglong::utils
