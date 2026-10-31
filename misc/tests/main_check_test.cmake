# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Exercise main-check.sh with a failing config check at both check levels.
# ldd-check is skipped so the test does not depend on a real application tree.
set(CMAKE_INSTALL_FULL_LIBEXECDIR "${TEST_DIR}/install/libexec")
set(helper_dir
    "${CMAKE_INSTALL_FULL_LIBEXECDIR}/linglong/builder/helper")
file(MAKE_DIRECTORY "${helper_dir}")

configure_file(
  "${SOURCE_DIR}/libexec/linglong/builder/helper/main-check.sh"
  "${TEST_DIR}/main-check.sh" @ONLY)
configure_file(
  "${SOURCE_DIR}/tests/config-check-fail.sh"
  "${helper_dir}/config-check.sh"
  COPYONLY
  FILE_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)

execute_process(
  COMMAND /bin/bash "${TEST_DIR}/main-check.sh" --skip-ldd-check 1
  RESULT_VARIABLE level1_result
  OUTPUT_VARIABLE level1_output
  ERROR_VARIABLE level1_error)
if(NOT level1_result EQUAL 1)
  message(
    FATAL_ERROR
      "Level 1 must fail on a config check failure, got ${level1_result}. "
      "Output:\n${level1_output}${level1_error}")
endif()
string(FIND "${level1_output}" "Error: application configure check failed"
       level1_error_message)
if(level1_error_message EQUAL -1)
  message(
    FATAL_ERROR
      "Level 1 must report the failure as an error. Output:\n${level1_output}")
endif()
string(FIND "${level1_output}" "Warning: application configure check failed"
       level1_warning)
if(NOT level1_warning EQUAL -1)
  message(
    FATAL_ERROR
      "Level 1 must not log a fatal failure as a warning. Output:\n${level1_output}")
endif()

execute_process(
  COMMAND /bin/bash "${TEST_DIR}/main-check.sh" --skip-ldd-check 0
  RESULT_VARIABLE level0_result
  OUTPUT_VARIABLE level0_output
  ERROR_VARIABLE level0_error)
if(NOT level0_result EQUAL 0)
  message(
    FATAL_ERROR
      "Level 0 must ignore a config check failure, got ${level0_result}. "
      "Output:\n${level0_output}${level0_error}")
endif()
string(FIND "${level0_output}" "Warning: application configure check failed"
       level0_warning)
if(level0_warning EQUAL -1)
  message(
    FATAL_ERROR
      "Level 0 must keep warning about the failure. Output:\n${level0_output}")
endif()
string(FIND "${level0_output}" "Error: application configure check failed"
       level0_error_message)
if(NOT level0_error_message EQUAL -1)
  message(
    FATAL_ERROR
      "Level 0 must not report an ignored failure as an error. Output:\n${level0_output}")
endif()

message(STATUS "main-check config failure handling passed")