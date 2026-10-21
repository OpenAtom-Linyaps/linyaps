# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  SOURCES
  ./src/main.cpp
  ./src/transform_old_exec.cpp
  OUTPUT_NAME
  ${LINGLONG_CLI_BIN}
  LINK_LIBRARIES
  PUBLIC
  linglong::linglong)
