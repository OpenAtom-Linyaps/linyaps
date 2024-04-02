# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(
  Qt5
  COMPONENTS Test
  REQUIRED)

pfl_add_executable(
  OUTPUT_NAME
  http_client_tests
  DISABLE_INSTALL
  SOURCES
  src/ostree_repo_test.cpp
  src/repo_client_test.cpp
  src/mock-network.h
  src/mock-network.cpp
  LINK_LIBRARIES
  PRIVATE
  Qt5::Test
  linglong::linglong)

add_test(NAME http_client_tests COMMAND http_client_tests)
