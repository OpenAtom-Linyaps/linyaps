# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  # find -regex '\.\/.+\.[ch]\(pp\)?' -type f -printf '%P\n'| sort
  src/linglong/api/dbus/v1/dbus_peer.cpp
  src/linglong/api/dbus/v1/dbus_peer.h
  src/linglong/api/dbus/v1/migrate.cpp
  src/linglong/api/dbus/v1/migrate.h
  src/linglong/api/dbus/v1/package_manager.cpp
  src/linglong/api/dbus/v1/package_manager.h
  src/linglong/api/dbus/v1/task.cpp
  src/linglong/api/dbus/v1/task.h
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  Qt5::Core
  Qt5::DBus)

function(linglong_add_dbus_interface target xml basename) # include

  set_source_files_properties(${xml} NO_NAMESPACE ON)

  if(${ARGC} GREATER 3)
    set(include "${ARGV3}")
    if(include)
      set_source_files_properties(${xml} PROPERTIES INCLUDE ${include})
    endif()
  endif()

  set(INTERFACE_SOURCES)
  set(Qt5DBus_QDBUSXML2CPP_EXECUTABLE qdbusxml2cpp)
  qt5_add_dbus_interface(INTERFACE_SOURCES ${xml} ${basename})
  target_sources(${target} PRIVATE ${INTERFACE_SOURCES})
endfunction()

set(real_dbus_api)
get_real_target_name(real_dbus_api linglong::dbus-api)
target_include_directories(${real_dbus_api} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

linglong_add_dbus_interface(
  ${real_dbus_api}
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  src/linglong/api/dbus/v1/gen_org_deepin_linglong_packagemanager1)

linglong_add_dbus_interface(
  ${real_dbus_api}
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Migrate1.xml
  src/linglong/api/dbus/v1/gen_org_deepin_linglong_migrate1)

linglong_add_dbus_interface(
  ${real_dbus_api} ${PROJECT_SOURCE_DIR}/api/dbus/org.freedesktop.DBus.Peer.xml
  ${real_dbus_api} ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Task1.xml
  src/linglong/api/dbus/v1/gen_org_deepin_linglong_task1)

linglong_add_dbus_interface(
  ${real_dbus_api} ${PROJECT_SOURCE_DIR}/api/dbus/org.freedesktop.DBus.Peer.xml
  src/linglong/api/dbus/v1/gen_org_freedesktop_dbus_peer)
