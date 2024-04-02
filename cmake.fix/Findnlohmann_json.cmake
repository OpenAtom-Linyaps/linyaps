# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

try_compile(
  nlohmann_json_FOUND ${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json_compile_test
  ${CMAKE_CURRENT_LIST_DIR}/nlohmann_json_compile_test.cpp)

if(NOT nlohmann_json_FOUND)
  return()
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  add_library(nlohmann_json INTERFACE)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  install(TARGETS nlohmann_json EXPORT "${PROJECT_NAME}")
endif()
