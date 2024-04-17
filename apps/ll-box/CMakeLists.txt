# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pkg_check_modules(SECCOMP REQUIRED IMPORTED_TARGET libseccomp)

pfl_add_executable(
  SOURCES
  ./src/main.cpp
  ./src/loader.cpp
  ./src/util/common.cpp
  ./src/util/filesystem.cpp
  ./src/util/semaphore.cpp
  ./src/util/debug/debug.cpp
  ./src/util/platform.cpp
  ./src/util/logger.cpp
  ./src/util/message_reader.cpp
  ./src/container/container.cpp
  ./src/container/mount/host_mount.cpp
  ./src/container/mount/filesystem_driver.cpp
  ./src/container/seccomp.cpp
  OUTPUT_NAME
  ll-box
  LINK_LIBRARIES
  PUBLIC
  nlohmann_json::nlohmann_json
  linglong::ocppi
  PkgConfig::SECCOMP
  COMPILE_OPTIONS
  PRIVATE -DJSON_USE_IMPLICIT_CONVERSIONS=0)
