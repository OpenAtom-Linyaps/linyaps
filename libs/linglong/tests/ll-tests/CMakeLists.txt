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
  src/common/tempdir.h
  src/linglong/builder/linglong_builder_test.cpp
  src/linglong/builder/source_fetcher_test.cpp
  src/linglong/cli/cli_test.cpp
  src/linglong/common/strings_test.cpp
  src/linglong/common/xdg_test.cpp
  src/linglong/mocks/command_mock.h
  src/linglong/mocks/layer_packager_mock.h
  src/linglong/mocks/linglong_builder_mock.h
  src/linglong/mocks/ostree_repo_mock.h
  src/linglong/mocks/uab_file_mock.h
  src/linglong/package/architecture_test.cpp
  src/linglong/package/fallback_version_test.cpp
  src/linglong/package/layer_packager_test.cpp
  src/linglong/package_manager/action_test.cpp
  src/linglong/package_manager/task_test.cpp
  src/linglong/package_manager/task_queue_test.cpp
  src/linglong/package_manager/package_update_test.cpp
  src/linglong/package_manager/ref_installation_test.cpp
  src/linglong/package_manager/uab_installation_test.cpp
  src/linglong/package/reference_test.cpp
  src/linglong/package/semver_compare_test.cpp
  src/linglong/package/semver_increment_test.cpp
  src/linglong/package/semver_prerelease_test.cpp
  src/linglong/package/semver_serialization_test.cpp
  src/linglong/package/semver_version_test.cpp
  src/linglong/package/uab_file_test.cpp
  src/linglong/package/version_test.cpp
  src/linglong/package/versionv2_test.cpp
  src/linglong/repo/client_factory_test.cpp
  src/linglong/repo/config_test.cpp
  src/linglong/repo/ostree_repo_test.cpp
  src/linglong/utils/bash_command_helper_test.cpp
  src/linglong/utils/command_test.cpp
  src/linglong/utils/error/error_test.cpp
  src/linglong/utils/file.cpp
  src/linglong/utils/filelock_test.cpp
  src/linglong/utils/gkeyfile_wrapper_test.cpp
  src/linglong/utils/log.cpp
  src/linglong/utils/namespce.cpp
  src/linglong/utils/packageinfo_handler_test.cpp
  src/linglong/utils/sha256_test.cpp
  src/linglong/utils/transaction_test.cpp
  src/linglong/utils/xdg/directory_test.cpp
  src/main.cpp

  # ll-driver-detect tests
  ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src/nvidia_driver_detector.cpp
  ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src/driver_detection_config.cpp
  ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src/dbus_notifier.cpp
  ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src/application_singleton.cpp
  ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src/driver_detection_manager.cpp
  src/apps/ll-driver-detect/driver_detector_test.cpp
  src/apps/ll-driver-detect/nvidia_driver_detector_test.cpp
  src/apps/ll-driver-detect/driver_detection_config_test.cpp
  src/apps/ll-driver-detect/application_singleton_test.cpp
  src/apps/ll-driver-detect/driver_detection_manager_test.cpp
  src/apps/ll-driver-detect/dbus_notifier_test.cpp
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  LINK_LIBRARIES
  PRIVATE
  GTest::gtest
  GTest::gmock
  linglong::linglong
  PkgConfig::CRYPTO)

include(GoogleTest)
get_real_target_name(tests linglong::linglong::ll-tests)
gtest_discover_tests(${tests} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# FIXME: we should'n include header directly
target_include_directories(${tests}
                           PUBLIC ${PROJECT_SOURCE_DIR}/apps/uab/header/src)
target_include_directories(${tests}
                           PUBLIC ${PROJECT_SOURCE_DIR}/apps/ll-driver-detect/src)
