# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(BOX_BIN_NAME "ll-box")

if(${STATIC_BOX})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  message(STATUS "build ll-box statically")
  set(BOX_BIN_NAME "ll-box-static")
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

pkg_search_module(SECCOMP REQUIRED IMPORTED_TARGET libseccomp)

pfl_add_executable(
  SOURCES
  src/container/container.cpp
  src/container/container.h
  src/container/helper.h
  src/container/helper.cpp
  src/container/mount/filesystem_driver.cpp
  src/container/mount/filesystem_driver.h
  src/container/mount/host_mount.cpp
  src/container/mount/host_mount.h
  src/container/seccomp.cpp
  src/container/seccomp.h
  src/main.cpp
  src/util/common.cpp
  src/util/common.h
  src/util/debug/debug.cpp
  src/util/debug/debug.h
  src/util/filesystem.cpp
  src/util/filesystem.h
  src/util/json.h
  src/util/logger.cpp
  src/util/logger.h
  src/util/macro.h
  src/util/message_reader.cpp
  src/util/message_reader.h
  src/util/oci_runtime.h
  src/util/platform.cpp
  src/util/platform.h
  src/util/semaphore.cpp
  src/util/semaphore.h
  src/util/util.h
  OUTPUT_NAME
  ${BOX_BIN_NAME}
  LINK_LIBRARIES
  PUBLIC
  nlohmann_json::nlohmann_json
  linglong::ocppi
  PkgConfig::SECCOMP
  COMPILE_OPTIONS
  PRIVATE
  -DJSON_USE_IMPLICIT_CONVERSIONS=0)

if(${STATIC_BOX})
  target_link_libraries(PkgConfig::SECCOMP
                        INTERFACE ${SECCOMP_STATIC_LIBRARIES})

  include(GNUInstallDirs)
  file(CREATE_LINK ${CMAKE_INSTALL_FULL_BINDIR}/ll-box-static
       ${CMAKE_CURRENT_BINARY_DIR}/ll-box SYMBOLIC)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ll-box
          DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
endif()
