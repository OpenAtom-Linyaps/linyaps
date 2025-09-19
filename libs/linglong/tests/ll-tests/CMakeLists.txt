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
  src/linglong/package/fallback_version_test.cpp
  src/linglong/package/reference_test.cpp
  src/linglong/package/version_test.cpp
  src/linglong/package/versionv2_test.cpp
  src/linglong/package/semver_compare_test.cpp
  src/linglong/package/semver_increment_test.cpp
  src/linglong/package/semver_prerelease_test.cpp
  src/linglong/package/semver_serialization_test.cpp
  src/linglong/package/semver_version_test.cpp
  src/linglong/package/uab_file_test.cpp
  src/linglong/package/layer_packager_test.cpp
  src/linglong/builder/source_fetcher_test.cpp
  src/linglong/mocks/command_mock.h
  src/linglong/mocks/ostree_repo_mock.h
  src/linglong/utils/error/result_test.cpp
  src/linglong/utils/namespce.cpp
  src/linglong/utils/log.cpp
  src/linglong/utils/sha256_test.cpp
  src/linglong/utils/transaction_test.cpp
  src/linglong/utils/command_test.cpp
  src/linglong/utils/bash_command_helper_test.cpp
  src/linglong/repo/config_test.cpp
  src/linglong/repo/ostree_repo_test.cpp
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
