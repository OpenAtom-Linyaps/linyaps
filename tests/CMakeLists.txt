# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

CPMFindPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  GIT_TAG v1.14.0
  VERSION 1.12.1
  OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
  FIND_PACKAGE_ARGUMENTS "NAMES GTest"
  GIT_SHALLOW ON
  EXCLUDE_FROM_ALL ON)

pfl_add_executable(
  INTERNAL
  SOURCES
  ./src/main.cpp
  ./src/module/config/config_test.cpp
  ./src/module/package/package_info_test.cpp
  ./src/module/package/ref_test.cpp
  ./src/module/qserializer/manifest_test.cpp
  ./src/module/qserializer/object.cpp
  ./src/module/qserializer/object.h
  ./src/module/qserializer/test.cpp
  ./src/module/runtime/app_test.cpp
  ./src/module/util/error_test.cpp
  ./src/module/util/fs_test.cpp
  ./src/module/util/http_client_test.cpp
  ./src/module/util/oci/distribution_test.cpp
  ./src/module/util/runner_test.cpp
  ./src/module/util/uuid_test.cpp
  ./src/module/util/xdg_test.cpp
  ./src/utils/serialize/json.cpp
  ./src/utils/serialize/TestStruct.cpp
  ./src/utils/serialize/TestStruct.h
  ./src/utils/serialize/yaml.cpp
  ./src/linglong/api/dbus/v1/mock_app_manager.h
  ./src/linglong/cli/cli_test.cpp
  ./src/linglong/package/version_test.cpp
  ./src/linglong/package/version_range_test.cpp
  ./src/linglong/package/reference_test.cpp
  ./src/linglong/cli/dbus_reply.h
  ./src/linglong/utils/xdg/desktop_entry_test.cpp
  ./src/linglong/utils/error/result_test.cpp
  ./src/linglong/repo/ostree_repo_test.cpp
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PUBLIC
  GTest::gmock
  linglong::linglong
  Qt::DBusPrivate)

include(GoogleTest)
get_real_target_name(linglong_tests linglong::linglong::tests)
message("${linglong_tests}")
gtest_discover_tests(${linglong_tests}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(src/qt)
