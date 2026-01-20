# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_library(
  MERGED_HEADER_PLACEMENT
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  SOURCES
  # find -regex '\.\/*.+\.[ch]\(pp\)?\(.in\)?' -type f -printf '%P\n'| sort
  src/linglong/common/constants.h
  src/linglong/common/dir.cpp
  src/linglong/common/dir.h
  src/linglong/common/display.cpp
  src/linglong/common/display.h
  src/linglong/common/error.h
  src/linglong/common/error.cpp
  src/linglong/common/socket.cpp
  src/linglong/common/socket.h
  src/linglong/common/strings.cpp
  src/linglong/common/strings.h
  src/linglong/common/xdg.cpp
  src/linglong/common/xdg.h
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  stdc++fs
  tl::expected)
