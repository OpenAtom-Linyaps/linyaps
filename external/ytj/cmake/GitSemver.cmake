# SPDX-FileCopyrightText: 2023 Chen Linxuan <me@black-desk.cn>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

get_property(
  GITSEMVER_INITIALIZED GLOBAL ""
  PROPERTY GITSEMVER_INITIALIZED
  SET)
if(GITSEMVER_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY GITSEMVER_INITIALIZED true)

function(gitsemver_message)
  message(STATUS "GitSemver: " ${ARGN})
endfunction()

message(STATUS "GitSemver: --==Version: v0.1.3==--")

# GitSemver will write the result to varname if it successfully get version
# string from git repository.
function(GitSemver varname)
  if(NOT PROJECT_IS_TOP_LEVEL)
    return()
  endif()

  gitsemver_message("Getting version from git repository ...")

  find_package(Git)
  if(NOT GIT_FOUND)
    gitsemver_message("Failed: git executable not found.")
    return()
  endif()
  find_program(SED_EXECUTABLE NAMES sed)
  if(NOT SED_EXECUTABLE)
    gitsemver_message("Failed: sed executable not found.")
    return()
  endif()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --match v* --long --dirty
    COMMAND ${SED_EXECUTABLE} -e s/-\\\([[:digit:]]\\+\\\)-g/+\\1\\./
    COMMAND ${SED_EXECUTABLE} -e s/-dirty\$/\\.dirty/
    COMMAND ${SED_EXECUTABLE} -e s/+0\\.[^\\.]\\+\\.\\?/+/
    COMMAND ${SED_EXECUTABLE} -e s/^v//
    COMMAND ${SED_EXECUTABLE} -e s/+\$//
    OUTPUT_VARIABLE ${varname} RESULTS_VARIABLE rets
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

  foreach(ret ${rets})
    if(NOT ret EQUAL 0)
      gitsemver_message("Failed: command failed.")
      return()
    endif()
  endforeach()

  gitsemver_message("${${varname}}")

  set(${varname}
      ${${varname}}
      PARENT_SCOPE)
endfunction()
