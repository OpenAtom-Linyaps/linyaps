# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  linglong
  SUBDIRS
  api
  utils
  dbus_ipc
  package
  repo
  job_manager
  runtime
  util
  builder
  package_manager
  service
  system_helper
  adaptors
  DEPENDENCIES
  tl::expected
)
