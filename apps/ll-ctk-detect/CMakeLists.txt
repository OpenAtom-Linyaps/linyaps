# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  OUTPUT_NAME
  ll-ctk-detect
  LIBEXEC
  linglong
  SOURCES
  ./src/main.cpp
  ./src/application_singleton.cpp
  ./src/container_tool_config.cpp
  ./src/container_tool_detector.cpp
  ./src/dbus_notifier.cpp
  ./src/deb_installer.cpp
  ./src/device_detector_registry.cpp
  ./src/nvidia_device_detector.cpp
  LINK_LIBRARIES
  PRIVATE
  linglong::linglong)
