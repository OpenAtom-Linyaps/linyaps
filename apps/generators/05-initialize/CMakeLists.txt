# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  OUTPUT_NAME
  05-initialize
  LIBEXEC
  linglong
  SOURCES
  src/main.cpp
  LINK_LIBRARIES
  PRIVATE
  nlohmann_json::nlohmann_json
  stdc++fs)
