# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_link_options(-static -static-libgcc -static-libstdc++)

pfl_add_executable(
  OUTPUT_NAME
  40-host-ipc-static
  LIBEXEC
  linglong
  SOURCES
  src/main.cpp
  LINK_LIBRARIES
  PUBLIC
  nlohmann_json::nlohmann_json
  stdc++fs)
