# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_library(FOUND_erofsfuse_STATIC "erofsfuse")

if("${FOUND_erofsfuse_STATIC}" STREQUAL "")
  message(SEND_ERROR "liberofsfuse.a not fount, skip uab-header")
endif()

get_filename_component(ABS_FILE ${FOUND_erofsfuse_STATIC} ABSOLUTE)
message(STATUS "found liberofsfuse.a: ${ABS_FILE}")

find_package(libdeflate REQUIRED COMPONENTS libdeflate_static)

pkg_search_module(elf_static REQUIRED IMPORTED_TARGET libelf)
pkg_search_module(lz4 REQUIRED IMPORTED_TARGET liblz4)
pkg_search_module(lzma REQUIRED IMPORTED_TARGET liblzma)
pkg_search_module(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_search_module(FUSE REQUIRED IMPORTED_TARGET fuse)
pkg_search_module(SELINUX REQUIRED IMPORTED_TARGET libselinux)
pkg_search_module(PCRE2-8 REQUIRED IMPORTED_TARGET libpcre2-8)
pkg_search_module(CRYPTO REQUIRED IMPORTED_TARGET libcrypto)
pkg_search_module(ZSTD REQUIRED IMPORTED_TARGET libzstd)

add_link_options(-static -static-libgcc -static-libstdc++)

pfl_add_executable(
  DISABLE_INSTALL
  SOURCES
  ./src/main.cpp
  OUTPUT_NAME
  uab-header
  LINK_LIBRARIES
  PRIVATE
  ${ABS_FILE}
  PkgConfig::elf_static
  PkgConfig::lz4
  PkgConfig::lzma
  PkgConfig::ZLIB
  PkgConfig::FUSE
  PkgConfig::SELINUX
  PkgConfig::PCRE2-8
  PkgConfig::CRYPTO
  PkgConfig::ZSTD
  libdeflate::libdeflate_static
  nlohmann_json::nlohmann_json)

include(GNUInstallDirs)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uab-header
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong/builder/uab)
