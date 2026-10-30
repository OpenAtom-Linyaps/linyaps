# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Configure a private installation without creating the repository directory.
# Spaces in both paths also exercise quoting in the installed shell scripts.
set(CMAKE_INSTALL_PREFIX "${TEST_DIR}/install prefix")
set(LINGLONG_ROOT "${TEST_DIR}/missing repo")
set(LINGLONG_EXPORT_PATH "share")
if(EXISTS "${LINGLONG_ROOT}")
  message(FATAL_ERROR "Test requires a missing repository: ${LINGLONG_ROOT}")
endif()

foreach(script generate-path.sh generate-xdg-data-dirs.sh)
  configure_file(
    "${SOURCE_DIR}/lib/linglong/${script}"
    "${CMAKE_INSTALL_PREFIX}/lib/linglong/${script}" @ONLY)
endforeach()

configure_file(
  "${SOURCE_DIR}/lib/systemd/system-environment-generators/61-linglong"
  "${TEST_DIR}/61-linglong" @ONLY)
configure_file("${SOURCE_DIR}/script/linglong.sh"
               "${TEST_DIR}/linglong.sh" @ONLY)

execute_process(
  COMMAND /bin/sh "${SOURCE_DIR}/tests/environment_test.sh" "${TEST_DIR}"
          "${LINGLONG_ROOT}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Environment script tests failed: ${result}")
endif()
