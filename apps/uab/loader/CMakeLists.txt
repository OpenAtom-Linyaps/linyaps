# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_link_options(-static -static-libgcc -static-libstdc++)

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  ./src/main.cpp
  OUTPUT_NAME
  uab-loader
  LINK_LIBRARIES
  PRIVATE
  linglong::ocppi
  linglong::api
  nlohmann_json::nlohmann_json)

include(GNUInstallDirs)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uab-loader
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong/builder/uab)
