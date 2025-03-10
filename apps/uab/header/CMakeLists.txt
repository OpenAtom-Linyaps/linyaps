# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# TODO: if erofsfuse and libdeflate provides .pc file or config.cmake, change it
# to pkg_search_modules/find_package

find_library(static_liberofsfuse "erofsfuse")
if("${static_liberofsfuse}" STREQUAL "static_liberofsfuse-NOTFOUND")
  message(FATAL_ERROR "liberofsfuse.a not found.")
endif()

get_filename_component(EROFSFUSE_ABS_FILE ${static_liberofsfuse} ABSOLUTE)
message(STATUS "found liberofsfuse.a: ${ABS_FILE}")

find_library(static_libdeflate "deflate")
if("${static_libdeflate}" STREQUAL "static_libdeflate-NOTFOUND")
  message(FATAL_ERROR "libdeflate.a not found")
endif()

get_filename_component(LIBDEFLATE_ABS_FILE ${static_libdeflate} ABSOLUTE)
message(STATUS "found libdeflate.a: ${LIBDEFLATE_ABS_FILE}")

pkg_search_module(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_search_module(lzma REQUIRED IMPORTED_TARGET liblzma)
pkg_search_module(FUSE3 REQUIRED IMPORTED_TARGET fuse3)
pkg_search_module(SELINUX REQUIRED IMPORTED_TARGET libselinux)
pkg_search_module(ZSTD REQUIRED IMPORTED_TARGET libzstd)
pkg_search_module(PCRE2_8 REQUIRED IMPORTED_TARGET libpcre2-8)

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  ./src/main.cpp
  ./src/sha256.h
  ./src/light_elf.h
  ./src/utils.h
  OUTPUT_NAME
  uab-header
  LINK_LIBRARIES
  PRIVATE
  linglong::api
  nlohmann_json::nlohmann_json
  PkgConfig::SELINUX
  PkgConfig::ZSTD
  PkgConfig::lz4
  PkgConfig::lzma
  PkgConfig::PCRE2_8
  PkgConfig::FUSE3
  ${EROFSFUSE_ABS_FILE}
  ${LIBDEFLATE_ABS_FILE}
  stdc++fs)

set(UAB_HEADER_TARGET)
get_real_target_name(UAB_HEADER_TARGET linglong::header)

target_link_options(${UAB_HEADER_TARGET} PRIVATE -static -static-libgcc
                    -static-libstdc++)

if(${AGGRESSIVE_UAB_SIZE})
  message(STATUS "minify size of uab header aggressively")
  target_compile_options(
    ${UAB_HEADER_TARGET} PRIVATE -Os -fno-asynchronous-unwind-tables -fno-rtti
                                 -fdata-sections -ffunction-sections -flto=auto)

  target_link_options(
    ${UAB_HEADER_TARGET} PRIVATE
    -Wl,--gc-sections,--as-needed,--strip-all,--exclude-libs,ALL -flto=auto)
endif()

include(GNUInstallDirs)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uab-header
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/linglong/builder/uab)
