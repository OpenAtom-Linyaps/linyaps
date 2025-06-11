# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  ./src/main.cpp
  OUTPUT_NAME
  uab-loader
  LINK_LIBRARIES
  PRIVATE
  linglong::api
  linglong::oci-cfg-generators)

set(UAB_LOADER_TARGET)
get_real_target_name(UAB_LOADER_TARGET linglong::loader)

target_link_options(${UAB_LOADER_TARGET} PRIVATE -static -static-libgcc
                    -static-libstdc++)

if(${AGGRESSIVE_UAB_SIZE})
  message(STATUS "minify size of uab loader aggressively")
  target_compile_options(
    ${UAB_LOADER_TARGET} PRIVATE -Os -fno-asynchronous-unwind-tables -fno-rtti
                                 -fdata-sections -ffunction-sections -flto=auto)

  target_link_options(
    ${UAB_LOADER_TARGET} PRIVATE
    -Wl,--gc-sections,--as-needed,--strip-all,--exclude-libs,ALL -flto=auto)
endif()

include(GNUInstallDirs)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uab-loader
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/linglong/builder/uab)
