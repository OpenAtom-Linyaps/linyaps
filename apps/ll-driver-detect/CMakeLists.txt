# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  OUTPUT_NAME
  ll-driver-detect
  LIBEXEC
  linglong
  SOURCES
  ./src/main.cpp
  ./src/driver_detection_config.cpp
  ./src/nvidia_driver_detector.cpp
  ./src/application_singleton.cpp
  ./src/driver_detection_manager.cpp
  ./src/dbus_notifier.cpp
  LINK_LIBRARIES
  PRIVATE
  CLI11::CLI11
  linglong::linglong
  linglong::utils)
