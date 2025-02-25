# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

if(NOT ENABLE_TESTING)
  return()
endif()

pfl_add_executable(
  OUTPUT_NAME
  ll-tests
  DISABLE_INSTALL
  SOURCES
  # find -regex '\./src/.+\.[ch]\(pp\)?' -type f -printf '%P\n'| sort
  src/linglong/package/reference_test.cpp
  src/linglong/package/version_range_test.cpp
  src/linglong/package/version_test.cpp
  src/linglong/utils/error/result_test.cpp
  src/linglong/utils/sha256_test.cpp
  src/linglong/utils/transaction_test.cpp
  src/linglong/utils/xdg/desktop_entry_test.cpp
  src/linglong/repo/config_test.cpp
  src/main.cpp
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PRIVATE
  GTest::gmock
  linglong::linglong
  PkgConfig::CRYPTO
  Qt${QT_VERSION_MAJOR}::DBusPrivate)

include(GoogleTest)
get_real_target_name(tests linglong::linglong::ll-tests)
gtest_discover_tests(${tests} WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# FIXME: we should'n include header directly
target_include_directories(${tests}
                           PUBLIC ${PROJECT_SOURCE_DIR}/apps/uab/header/src)
