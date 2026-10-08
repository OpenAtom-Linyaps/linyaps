# SPDX-FileCopyrightText: 2026 3219378872
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Also allow running the script tests without building the C++ tools.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.11.4)
  project(linglong-script-tests NONE)
  enable_testing()
endif()

if(CMAKE_VERSION VERSION_LESS "3.12")
  find_package(PythonInterp 3.6 REQUIRED)
  set(SCRIPT_TEST_PYTHON ${PYTHON_EXECUTABLE})
else()
  find_package(Python3 3.6 REQUIRED COMPONENTS Interpreter)
  set(SCRIPT_TEST_PYTHON ${Python3_EXECUTABLE})
endif()
add_test(NAME fetch-file-source
         COMMAND ${SCRIPT_TEST_PYTHON}
                 ${CMAKE_CURRENT_SOURCE_DIR}/fetch_file_source_test.py)
set_tests_properties(fetch-file-source PROPERTIES TIMEOUT 60)
