# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

function(
  linglong_add_dbus_adaptor
  target_name
  xml
  header
  parent_class
  basename
  classname
)

  set(DBUS_ADAPTOR_SOURCE)
  qt5_add_dbus_adaptor(
    DBUS_ADAPTOR_SOURCE
    ${xml}
    ${header}
    ${parent_class}
    ${basename}
    ${classname}
  )
  target_sources(${target_name} PRIVATE ${DBUS_ADAPTOR_SOURCE})
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

linglong_add_library(
  adaptors
  SUBDIRS
  app_manager
  package_manager
  job_manager
  system_helper
  # package_manager_helper
)
