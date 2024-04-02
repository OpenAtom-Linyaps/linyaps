#[[
SPDX-FileCopyrightText: 2023 Chen Linxuan <me@black-desk.cn>
SPDX-License-Identifier: LGPL-3.0-or-later
]]

#[[

# PFL.cmake

This project is design to build c++ project follows [the Pitchfork layout].

You can just copy the [PFL.cmake] file into your awesome project.

For futher information, check [examples] and comments.

[PFL.cmake]: https://github.com/black-desk/PFL.cmake/releases/latest/download/PFL.cmake

[the Pitchfork layout]: https://github.com/vector-of-bool/pitchfork/blob/develop/data/spec.bs

[examples]: https://github.com/black-desk/PFL.cmake/tree/master/examples

]]

# NOTE: RHEL 8 use cmake 3.11.4
cmake_minimum_required(VERSION 3.11.4 FATAL_ERROR)

get_property(
  PFL_INITIALIZED GLOBAL
  PROPERTY PFL_INITIALIZED
  SET)
if(PFL_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY PFL_INITIALIZED true)

set(_PFL_VERSION "v0.4.1")

set(_PFL_BUG_REPORT_MESSAGE
    [=[
PFL.cmake bug detected.
Please fire a bug report on https://github.com/black-desk/PFL.cmake/issues/new.
]=])

function(_pfl_bug)
  message(FATAL_ERROR "${_PFL_BUG_REPORT_MESSAGE}")
endfunction()

function(_pfl_str_join OUT_NAME SEP)
  foreach(STRING ${ARGN})
    if(NOT RESULT)
      set(RESULT ${STRING})
    else()
      set(RESULT "${RESULT}${SEP}${STRING}")
    endif()
  endforeach()

  set("${OUT_NAME}"
      "${RESULT}"
      PARENT_SCOPE)
endfunction()

set(_PFL_MESSAGE_PREFIX "PFL.cmake: ")

function(_pfl_message MODE)
  _pfl_str_join(MESSAGE " " ${ARGN})
  message(${MODE} "${_PFL_MESSAGE_PREFIX}${MESSAGE}")
endfunction()

function(_pfl_info)
  _pfl_message(STATUS ${ARGV})
endfunction()

function(_pfl_warn)
  _pfl_message(WARNING ${ARGV})
endfunction()

function(_pfl_fatal)
  _pfl_message(FATAL_ERROR ${ARGV})
endfunction()

_pfl_info("Version: ${_PFL_VERSION}")

set(_PFL_PATH_COMPONENT_VALIDATOR "^(_.*|.*__.*|.*_)\$")
function(_pfl_normalize_path_component OUT_NAME PATH_COMPONENT)
  set(NORMALIZED_PATH_COMPONENT "${PATH_COMPONENT}")
  if(NOT NORMALIZED_PATH_COMPONENT)
    _pfl_bug()
  endif()

  string(REPLACE " " "_" NORMALIZED_PATH_COMPONENT
                 "${NORMALIZED_PATH_COMPONENT}")
  string(REPLACE "-" "_" NORMALIZED_PATH_COMPONENT
                 "${NORMALIZED_PATH_COMPONENT}")
  string(REPLACE ":" "_" NORMALIZED_PATH_COMPONENT
                 "${NORMALIZED_PATH_COMPONENT}")

  if(NORMALIZED_PATH_COMPONENT MATCHES "${_PFL_PATH_COMPONENT_VALIDATOR}")
    return()
  endif()

  set("${OUT_NAME}"
      "${NORMALIZED_PATH_COMPONENT}"
      PARENT_SCOPE)
endfunction()

function(_pfl_check_called_from_project_source_dir FUNCTION_NAME)
  if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    return()
  endif()

  _pfl_fatal("${FUNCTION_NAME} must be called at \${PROJECT_SOURCE_DIR}")
endfunction()

function(_pfl_normalize_project_name OUT_NAME PROJECT_NAME)
  _pfl_normalize_path_component(NORMALIZED_PROJECT_NAME "${PROJECT_NAME}")

  if(NOT NORMALIZED_PROJECT_NAME)
    _pfl_fatal("Unsupported project name \"${PROJECT_NAME}\" detected."
               "Please rename your project.")
  endif()

  set("${OUT_NAME}"
      "${NORMALIZED_PROJECT_NAME}"
      PARENT_SCOPE)
endfunction()

#[[

# PFL_init

You should call this function in your top level CMakeLists.txt to
tell PFL.cmake some settings about your project.
Some variables starts with `PFL_` will be set after this function is called.

## Argument

- *boolean* `BUILD_SHARED_LIBS`
  Tell `PFL_add_library()` to default to `SHARED` libraries,
  instead of `STATIC` libraries, when called with no explicit library type;
- *boolean* `ENABLE_INSTALL`
  Whether to enable `install` rules for this project;
- *boolean* `ENABLE_TESTING`
  Whether to call `add_subdirectory` on SUBMODULE/tests/*;
- *boolean* `ENABLE_EXAMPLES`
  Whether to call add_subdirectory on SUBMODULE/examples/*;
- *boolean* `ENABLE_APPLICATIONS`
  Whether to call add_subdirectory on SUBMODULE/apps/*;
- *boolean* `ENABLE_EXTERNALS`
  Whether to call add_subdirectory on /externals/*;
- *list* `EXTERNALS`
  The list of directories in /externals contain dependencies
  which should be embed to this project.
  PFL.cmake will call `add_subdirectory` on them by order
  if `ENABLE_EXTERNALS` is `ON`.

## Example

```cmake
# At the very beginning of your top level CMakeLists.txt.

cmake_minimum_required(VERSION 3.11.4 FATAL_ERROR)

project(xxx LANGUAGES CXX)

if(CMAKE_VERSION VERSION_LESS 3.21)
  # https://www.scivision.dev/cmake-project-is-top-level/
  get_property(not_top DIRECTORY PROPERTY PARENT_DIRECTORY)
  if(NOT not_top)
    set(PROJECT_IS_TOP_LEVEL true)
  endif()
endif()

# Define some default options for your project:

option(xxx_BUILD_SHARED_LIBS "Default to build shared library of XXX" ${PROJECT_IS_TOP_LEVEL})
option(xxx_ENABLE_INSTALL "Enable install target of XXX" ${PROJECT_IS_TOP_LEVEL})
option(xxx_ENABLE_TESTING "Build and running test of XXX" ${PROJECT_IS_TOP_LEVEL})
option(xxx_ENABLE_EXAMPLES "Build examples of XXX" ${PROJECT_IS_TOP_LEVEL})
option(xxx_ENABLE_EXTERNALS "Enable external libraries of XXX" "${PROJECT_IS_TOP_LEVEL}")
option(xxx_ENABLE_APPLICATIONS "Build applications of XXX" ${PROJECT_IS_TOP_LEVEL})

if(${PROJECT_IS_TOP_LEVEL})
  set(xxx_EXTERNALS_DEFAULT "A;B;C")
endif()
option(xxx_EXTERNALS "List of libraries will be embed into XXX" ${XXX_EXTERNALS_DEFAULT})

include(./cmake/PFL.cmake)

PFL_init(
  BUILD_SHARED_LIBS "${xxx_BUILD_SHARED_LIBS}"
  ENABLE_INSTALL "${xxx_ENABLE_INSTALL}"
  ENABLE_TESTING "${xxx_ENABLE_TESTING}"
  ENABLE_EXAMPLES "${xxx_ENABLE_EXAMPLES}"
  ENABLE_EXTERNALS "${xxx_ENABLE_EXTERNALS}"
  ENABLE_APPLICATIONS "${xxx_ENABE_APPLICATIONS}"
  EXTERNALS "${xxx_EXTERNALS}"
)

# or just

PFL_init(AUTO)

```

]]
macro(PFL_init)
  _pfl_check_called_from_project_source_dir(PFL_init)

  cmake_parse_arguments(
    PFL_INIT
    "AUTO"
    "PROJECT_NAME;BUILD_SHARED_LIBS;ENABLE_INSTALL;ENABLE_TESTING;ENABLE_EXAMPLES;ENABLE_EXTERNALS;ENABLE_APPLICATIONS"
    "EXTERNALS"
    ${ARGN})

  if(PFL_INIT_AUTO)
    if(PFL_INIT_PROJECT_NAME
       OR PFL_INIT_BUILD_SHARED_LIBS
       OR PFL_INIT_ENABLE_INSTALL
       OR PFL_INIT_ENABLE_TESTING
       OR PFL_INIT_ENABLE_EXAMPLES
       OR PFL_INIT_ENABLE_EXTERNALS
       OR PFL_INIT_ENABLE_APPLICATIONS
       OR PFL_INIT_EXTERNALS)
      _pfl_fatal("No other parameters should be passed"
                 "if you are using PFL_init with AUTO.")
    endif()

    if(CMAKE_VERSION VERSION_LESS 3.21)
      # https://www.scivision.dev/cmake-project-is-top-level/
      get_property(
        not_top
        DIRECTORY
        PROPERTY PARENT_DIRECTORY)
      if(NOT not_top)
        set(PROJECT_IS_TOP_LEVEL true)
      endif()
    endif()

    _pfl_normalize_project_name(PFL_INIT_PROJECT_NAME "${PROJECT_NAME}")

    set(PFL_INIT_BUILD_SHARED_LIBS ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_BUILD_SHARED_LIBS)
      set(PFL_INIT_BUILD_SHARED_LIBS
          ${${PFL_INIT_PROJECT_NAME}_BUILD_SHARED_LIBS})
    endif()

    set(PFL_INIT_ENABLE_INSTALL ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_ENABLE_INSTALL)
      set(PFL_INIT_ENABLE_INSTALL ${${PFL_INIT_PROJECT_NAME}_ENABLE_INSTALL})
    endif()

    set(PFL_INIT_ENABLE_TESTING ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_ENABLE_TESTING)
      set(PFL_INIT_ENABLE_TESTING ${${PFL_INIT_PROJECT_NAME}_ENABLE_TESTING})
    endif()

    set(PFL_INIT_ENABLE_EXAMPLES ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_ENABLE_EXAMPLES)
      set(PFL_INIT_ENABLE_EXAMPLES ${${PFL_INIT_PROJECT_NAME}_ENABLE_EXAMPLES})
    endif()

    set(PFL_INIT_ENABLE_EXTERNALS ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_ENABLE_EXTERNALS)
      set(PFL_INIT_ENABLE_EXTERNALS
          ${${PFL_INIT_PROJECT_NAME}_ENABLE_EXTERNALS})
    endif()

    set(PFL_INIT_ENABLE_APPLICATIONS ${PROJECT_IS_TOP_LEVEL})
    if(DEFINED ${PFL_INIT_PROJECT_NAME}_ENABLE_APPLICATIONS)
      set(PFL_INIT_ENABLE_APPLICATIONS
          ${${PFL_INIT_PROJECT_NAME}_ENABLE_APPLICATIONS})
    endif()

    set(PFL_INIT_EXTERNALS ${${PFL_INIT_PROJECT_NAME}_EXTERNALS})
  endif()

  if(NOT PFL_INIT_PROJECT_NAME)
    _pfl_normalize_project_name(PFL_INIT_PROJECT_NAME "${PROJECT_NAME}")
  endif()

  _pfl_normalize_project_name(NORMALIZED_PFL_INIT_PROJECT_NAME
                              "${PFL_INIT_PROJECT_NAME}")
  if(NOT NORMALIZED_PFL_INIT_PROJECT_NAME STREQUAL PFL_INIT_PROJECT_NAME)
    _pfl_fatal(
      "PROJECT_NAME of PFL_init (\"${PFL_INIT_PROJECT_NAME}\")"
      "should be normalized to \"${NORMALIZED_PFL_INIT_PROJECT_NAME}\".")
  endif()
  set(PFL_PROJECT_NAME ${PFL_INIT_PROJECT_NAME})

  set(_PFL_PATH)
  list(APPEND _PFL_PATH ${PFL_INIT_PROJECT_NAME})
  set(_PFL_PATH ${_PFL_PATH})

  set(_PFL_BUILD_SHARED_LIBS ${PFL_INIT_BUILD_SHARED_LIBS})
  set(_PFL_ENABLE_INSTALL ${PFL_INIT_ENABLE_INSTALL})
  set(_PFL_ENABLE_TESTING ${PFL_INIT_ENABLE_TESTING})
  set(_PFL_ENABLE_EXAMPLES ${PFL_INIT_ENABLE_EXAMPLES})
  set(_PFL_ENABLE_EXTERNALS ${PFL_INIT_ENABLE_EXTERNALS})
  set(_PFL_ENABLE_APPLICATIONS ${PFL_INIT_ENABLE_APPLICATIONS})

  if(pfl_INIT_ENABLE_TESTING)
    enable_testing()
  endif()

  if(PFL_INIT_ENABLE_EXTERNALS)
    foreach(EXTERNAL ${PFL_INIT_EXTERNALS})
      add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/${EXTERNAL}
                       EXCLUDE_FROM_ALL)
    endforeach()
  endif()
endmacro()

set(PFL_add_libraries_default_cmake_config
    [=[
@PACKAGE_INIT@

list(APPEND CMAKE_MODULE_PATH "@PACKAGE_cmakeModulesDir@")

if(${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS)
  set(Components ${${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS})
else()
  set(Components @PFL_COMPONMENTS@)
endif()

@PFL_EXPORT_NAME_OF_COMPONMENTS@

# Check all required components are available before trying to load any
foreach(Component ${Components})
  if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_${Component})
    continue();
  endif()

  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${EXPORT_NAME_OF_${Component}}.cmake)
    continue();
  endif()

  set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
      "Missing required component: ${Component}")

  set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)
  return()
endforeach()

foreach(Component ${Components})
  include(${CMAKE_CURRENT_LIST_DIR}/${EXPORT_NAME_OF_${Component}}.cmake OPTIONAL)
endforeach()
]=])

#[[

# PFL_add_libraries

This function is used to add libraries in the `libs` directory
for a large c++ project constructed with multiple module.

It should only be called in `${PROJECT_SOURCE_DIR}`,
`libs` in submodule root is not allowed.

If `${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt` exists
and testing is enabled in your project,
PFL.cmake will call add_subdirectory to build test of your libraries.

## Argument

- *list* `LIBS`
  List of directories under `libs` that should be considered as submodule roots.
- *list* `APPS`
  Directories in `/apps` that contain source code of application.
  If application targets is enabled in your project,
  which can be done by set `ENABLE_APPLICATION` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.
- *list* `EXAMPLES`
  Directories in `/examples` that contain source code of examples.
  If example targets is enabled in your project,
  which can be done by set `ENABLE_EXAMPLE` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.
- *list* `TESTS`
  Directories in `/tests` that contain source code of tests.
  If test targets is enabled in your project,
  which can be done by set `ENABLE_TESTING` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.

## Example

```cmake
# ...

# After PFL_init called in your top level CMakeLists.txt

PFL_add_libraries(LIBRARIES A B C)
```
This make PFL.cmake set `PFL_LIBRARIES` to `A;B;C`
and call add_subdirectory on `${CMAKE_CURRENT_SOURCE_DIR}/libs/A`,
`${CMAKE_CURRENT_SOURCE_DIR}/libs/B` and `${CMAKE_CURRENT_SOURCE_DIR}/libs/C`.

If the install target of your project is enabled,
which can be done by set `ENABLE_INSTALL` to true when call `PFL_init`,
PFL.cmake will configure the cmake config file for your project
then generate version file.

You can override the default cmake config file by providing
`${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake/${PROJECT_NAME}Config.cmake.in`.

]]
function(PFL_add_libraries)
  _pfl_check_called_from_project_source_dir(PFL_add_libraries)

  cmake_parse_arguments(PFL_ADD_LIBRARIES "" "VERSION"
                        "LIBS;EXAMPLES;APPS;TESTS" ${ARGN})

  _pfl_info("Adding libraries at ${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT PFL_ADD_LIBRARIES_VERSION)
    set(PFL_ADD_LIBRARIES_VERSION ${PROJECT_VERSION})
  endif()

  set_property(GLOBAL PROPERTY PFL_COMPONMENTS)
  set_property(GLOBAL PROPERTY PFL_EXPORT_NAME_OF_COMPONMENTS)
  foreach(LIB ${PFL_ADD_LIBRARIES_LIBS})
    add_subdirectory(libs/${LIB})
  endforeach()

  _pfl_add_tests(${PFL_ADD_LIBRARIES_TESTS})
  _pfl_add_examples(${PFL_ADD_LIBRARIES_EXAMPLES})
  _pfl_add_apps(${PFL_ADD_LIBRARIES_APPS})

  if(NOT _PFL_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  set(CONFIG_FILE misc/cmake/${PFL_PROJECT_NAME}Config.cmake)

  set(CONFIG_FILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}.in)
  if(NOT EXISTS ${CONFIG_FILE_IN})
    write_file(${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in
               ${PFL_add_libraries_default_cmake_config})
    set(CONFIG_FILE_IN ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in)
  endif()

  include(CMakePackageConfigHelpers)
  # This will be used to replace @PACKAGE_cmakeModulesDir@
  set(cmakeModulesDir cmake)

  get_property(PFL_COMPONMENTS GLOBAL PROPERTY PFL_COMPONMENTS)
  get_property(PFL_EXPORT_NAME_OF_COMPONMENTS GLOBAL
               PROPERTY PFL_EXPORT_NAME_OF_COMPONMENTS)
  configure_package_config_file(
    ${CONFIG_FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
    PATH_VARS cmakeModulesDir
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PFL_PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  set(VERSION_FILE misc/cmake/${PFL_PROJECT_NAME}ConfigVersion.cmake)
  write_basic_package_version_file(
    ${VERSION_FILE}
    VERSION ${PFL_ADD_LIBRARIES_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
                ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE}
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PFL_PROJECT_NAME})
endfunction()

function(_pfl_add_tests)
  if(NOT _PFL_ENABLE_TESTING)
    return()
  endif()

  foreach(TEST ${ARGV})
    include(CTest)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST})
  endforeach()
endfunction()

function(_pfl_add_examples)
  if(NOT _PFL_ENABLE_EXAMPLES)
    return()
  endif()

  foreach(EXAMPLE ${ARGV})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/${EXAMPLE})
  endforeach()
endfunction()

function(_pfl_add_apps)
  if(NOT _PFL_ENABLE_APPLICATIONS)
    return()
  endif()

  foreach(APP ${ARGV})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/apps/${APP})
  endforeach()
endfunction()

function(_pfl_get_current_target_name OUT_NAME)
  if(CMAKE_CURRENT_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set("${OUT_NAME}"
        ${PFL_PROJECT_NAME}
        PARENT_SCOPE)
    return()
  endif()

  get_filename_component(NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  _pfl_normalize_path_component(NAME ${NAME})
  set("${OUT_NAME}"
      ${NAME}
      PARENT_SCOPE)
endfunction()

function(_pfl_handle_sources HEADERS_OUT SOURCES_OUT MERGED_PLACEMENT)
  set(SOURCES)
  set(HEADERS)

  foreach(SOURCE_FILE ${ARGN})
    if(SOURCE_FILE MATCHES "^./")
      string(REGEX REPLACE "^./" "" SOURCE_FILE ${SOURCE_FILE})
    endif()

    if(SOURCE_FILE MATCHES "\.in$")
      string(REGEX REPLACE "\.in$" "" OUT_FILE ${SOURCE_FILE})
      configure_file(${SOURCE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}
                     @ONLY)
      set(SOURCE_FILE ${OUT_FILE})
    endif()

    set(SOURCES_REGEXP "src\/.*\.(h(h|pp)?|c(c|pp|xx))\$")
    set(HEADERS_REGEXP "include\/.*\.h(h|pp)?\$")
    if(MERGED_PLACEMENT)
      set(SOURCES_REGEXP "src\/.*\.c(c|pp|xx)\$")
      set(HEADERS_REGEXP "src\/.*\.h(h|pp)?\$")
    endif()

    if(SOURCE_FILE MATCHES "${HEADERS_REGEXP}")
      set(TARGET_LIST HEADERS)
    elseif(SOURCE_FILE MATCHES "${SOURCES_REGEXP}")
      set(TARGET_LIST SOURCES)
    else()
      # FIXME: handle .inc
      _pfl_fatal("Unrecognized file type ${SOURCE_FILE}")
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE})
      list(APPEND ${TARGET_LIST} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE})
    else()
      list(APPEND ${TARGET_LIST} ${CMAKE_CURRENT_BINARY_DIR}/${SOURCE_FILE})
    endif()
  endforeach()

  set("${HEADERS_OUT}"
      ${HEADERS}
      PARENT_SCOPE)
  set("${SOURCES_OUT}"
      ${SOURCES}
      PARENT_SCOPE)
endfunction()

set(PFL_add_library_default_cmake_config
    [=[
@PACKAGE_INIT@

list(APPEND CMAKE_MODULE_PATH "@PACKAGE_cmakeModulesDir@")

include(CMakeFindDependencyMacro)

@PFL_FIND_DEPENDENCIES@

include(${CMAKE_CURRENT_LIST_DIR}/@PFL_TARGET_EXPORT_NAME@.cmake)
]=])
#[[

# PFL_add_library

This function is used to add a library,
target name of the library added is
`${PROJECT_NAME}::` + current source directory name.
This target will be an alias target.

PFL.cmake will automatically generate a `*Config.cmake` file for your library.

## Arguments

- *option* `DISABLE_INSTALL`
- *option* `MERGED_HEADER_PLACEMENT`
  Use merged header placement. This make all header files under src public.
- *string* `LIBRARY_TYPE`
  `STATIC`, `SHARED` or `HEADER_ONLY`.
  If no `LIBRARY_TYPE` is given the default is STATIC or SHARED
  based on the value of the BUILD_SHARED_LIBS variable.
  If not specified, cmake will automatically decide to build a static
- *string* `OUTPUT_NAME`
  The output file name of this library.
- *string* `VERSION`
  The version of this library.
- *string* `SOVERSION`
  The soversion of this library.
  **NOT** available for `HEADER_ONLY` and `STATIC` library.
- *list* `SOURCES`
  List of source files, include .in, sources and headers.
  We support add (possibly near-empty) cpp files
  as sources of header only library to check if that header is self-contained.
- *list* `TESTS`
  Directories in `/tests` that contain source code of tests.
  If test targets is enabled in your project,
  which can be done by set `ENABLE_TESTING` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.
- *list* `APPS`
  Directories in `/apps` that contain source code of application.
  If application targets is enabled in your project,
  which can be done by set `ENABLE_APPLICATION` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.
- *list* `EXAMPLES`
  Directories in `/examples` that contain source code of examples.
  If example targets is enabled in your project,
  which can be done by set `ENABLE_EXAMPLE` to true when call `PFL_init`,
  PFL.cmake will call add_subdirectory on these directory in order.
- *list* `LINK_LIBRARIES`
  Arguments passed to target_link_libraries.
- *list* `COMPILE_FEATURES`
  Arguments passed to target_compile_features.
- *list* `FIND_DEPENDENCY_ARGUMENTS`
  Arguments pass to `find_dependency()` in auto-generated `*Config.cmake` file.

## Example

Check [examples](https://github.com/black-desk/PFL.cmake/tree/master/examples)
online.

]]
function(pfl_add_library)
  cmake_parse_arguments(
    PFL_ADD_LIBRARY
    "DISABLE_INSTALL;MERGED_HEADER_PLACEMENT"
    "LIBRARY_TYPE;OUTPUT_NAME;VERSION;SOVERSION"
    "SOURCES;TESTS;APPS;EXAMPLES;LINK_LIBRARIES;COMPILE_FEATURES;FIND_DEPENDENCY_ARGUMENTS"
    ${ARGN})

  _pfl_get_current_target_name(TARGET_NAME)
  set(OLD_PFL_PATH ${_PFL_PATH})
  list(APPEND _PFL_PATH ${TARGET_NAME})
  _pfl_str_join(TARGET_FULL_NAME "__" ${_PFL_PATH})
  _pfl_str_join(TARGET_FULL_ALIAS_NAME "::" ${_PFL_PATH})
  _pfl_str_join(TARGET_FULL_PATH "/" ${_PFL_PATH})

  if(NOT PFL_ADD_LIBRARY_OUTPUT_NAME)
    set(PFL_ADD_LIBRARY_OUTPUT_NAME ${TARGET_FULL_NAME})

    if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
      set(PFL_ADD_LIBRARY_OUTPUT_NAME ${TARGET_NAME})
    endif()

    _pfl_warn("OUTPUT_NAME of ${TARGET_FULL_ALIAS_NAME} not set,"
              "using ${PFL_ADD_LIBRARY_OUTPUT_NAME}")
  endif()

  _pfl_info("Adding library ${TARGET_FULL_ALIAS_NAME}"
            "at ${CMAKE_CURRENT_SOURCE_DIR}")

  _pfl_handle_sources(
    HEADERS SOURCES ${PFL_ADD_LIBRARY_MERGED_HEADER_PLACEMENT}
    ${PFL_ADD_LIBRARY_SOURCES})

  if(NOT PFL_ADD_LIBRARY_LIBRARY_TYPE)
    set(PPFL_ADD_LIBRARY_LIBRARY_TYPE "STATIC")
    if(_PFL_BUILD_SHARED_LIBS)
      set(PPFL_ADD_LIBRARY_LIBRARY_TYPE "SHARED")
    endif()
  endif()

  set(LIBRARY_TYPE ${PFL_ADD_LIBRARY_LIBRARY_TYPE})
  set(PROPAGATE PUBLIC)
  if("${PFL_ADD_LIBRARY_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    set(LIBRARY_TYPE INTERFACE)
    set(PROPAGATE INTERFACE)
  endif()

  add_library(${TARGET_FULL_NAME} ${LIBRARY_TYPE})
  foreach(HEADER ${HEADERS})
    target_sources(${TARGET_FULL_NAME} ${PROPAGATE}
                                       $<BUILD_INTERFACE:${HEADER}>)
  endforeach()

  set(INCLUDE_PREFIX include)
  if(PFL_ADD_LIBRARY_MERGED_HEADER_PLACEMENT)
    set(INCLUDE_PREFIX src)
  endif()

  target_include_directories(
    ${TARGET_FULL_NAME}
    ${PROPAGATE}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_PREFIX}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/${INCLUDE_PREFIX}>)

  set(LIBRARY_NAME ${TARGET_FULL_NAME})
  if("${PFL_ADD_LIBRARY_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    add_library(${TARGET_FULL_NAME}__BUILD STATIC)
    target_link_libraries(${TARGET_FULL_NAME}__BUILD
                          PRIVATE ${TARGET_FULL_NAME})
    set(LIBRARY_NAME ${TARGET_FULL_NAME}__BUILD)
  endif()

  target_sources(${LIBRARY_NAME} PRIVATE ${SOURCES})

  target_include_directories(
    ${LIBRARY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${CMAKE_CURRENT_BINARY_DIR}/src)

  if(NOT "${PFL_ADD_LIBRARY_SOVERSION}" STREQUAL "")
    set_target_properties("${TARGET_FULL_NAME}"
                          PROPERTIES SOVERSION ${PFL_ADD_LIBRARY_SOVERSION})
  endif()
  if(NOT "${PFL_ADD_LIBRARY_VERSION}" STREQUAL "")
    set_target_properties("${TARGET_FULL_NAME}"
                          PROPERTIES VERSION ${PFL_ADD_LIBRARY_VERSION})
  endif()

  #set_target_properties("${TARGET_FULL_NAME}"
  #                      PROPERTIES OUTPUT_NAME ${PFL_ADD_LIBRARY_OUTPUT_NAME})

  if(NOT "${TARGET_FULL_ALIAS_NAME}" STREQUAL "${TARGET_FULL_NAME}")
    add_library("${TARGET_FULL_ALIAS_NAME}" ALIAS "${TARGET_FULL_NAME}")
  endif()

  if(PFL_ADD_LIBRARY_LINK_LIBRARIES)
    target_link_libraries(${TARGET_FULL_NAME} ${PFL_ADD_LIBRARY_LINK_LIBRARIES})
  endif()

  if(PFL_ADD_LIBRARY_COMPILE_FEATURES)
    target_compile_features(${TARGET_FULL_NAME}
                            ${PFL_ADD_LIBRARY_COMPILE_FEATURES})
  endif()

  _pfl_add_tests(${PFL_ADD_LIBRARY_TESTS})
  _pfl_add_examples(${PFL_ADD_LIBRARY_EXAMPLES})
  _pfl_add_apps(${PFL_ADD_LIBRARY_APPS})

  set(_PFL_PATH ${OLD_PFL_PATH})

  if(PFL_ADD_LIBRARY_DISABLE_INSTALL OR NOT _PFL_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  set(PROPAGATE PUBLIC)
  if("${PFL_ADD_LIBRARY_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    set(PROPAGATE INTERFACE)
  endif()

  foreach(FILE ${HEADERS})
    if(FILE MATCHES "${CMAKE_CURRENT_SOURCE_DIR}/include/.*")
      file(RELATIVE_PATH RELATIVE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/include
           ${FILE})
    elseif(FILE MATCHES "${CMAKE_CURRENT_BINARY_DIR}/include/.*")
      file(RELATIVE_PATH RELATIVE_FILE ${CMAKE_CURRENT_BINARY_DIR}/include
           ${FILE})
    elseif(FILE MATCHES "${CMAKE_CURRENT_SOURCE_DIR}/src/.*")
      file(RELATIVE_PATH RELATIVE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src ${FILE})
    elseif(FILE MATCHES "${CMAKE_CURRENT_BINARY_DIR}/src/.*")
      file(RELATIVE_PATH RELATIVE_FILE ${CMAKE_CURRENT_BINARY_DIR}/src ${FILE})
    else()
      _pfl_bug()
    endif()

    get_filename_component(DIR ${RELATIVE_FILE} DIRECTORY)
    install(
      FILES ${FILE}
      DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET_FULL_PATH}/${DIR})

    target_sources(
      "${TARGET_FULL_NAME}"
      ${PROPAGATE}
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}/${TARGET_FULL_PATH}/${RELATIVE_FILE}>
    )
  endforeach()

  target_include_directories(
    ${TARGET_FULL_NAME}
    ${PROPAGATE}
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_FULL_PATH}>)

  set(PFL_TARGET_EXPORT_NAME ${PFL_ADD_LIBRARY_OUTPUT_NAME})
  get_property(PFL_COMPONMENTS GLOBAL PROPERTY PFL_COMPONMENTS)
  list(APPEND PFL_COMPONMENTS ${TARGET_NAME})
  set_property(GLOBAL PROPERTY PFL_COMPONMENTS ${PFL_COMPONMENTS})

  get_property(PFL_EXPORT_NAME_OF_COMPONMENTS GLOBAL
               PROPERTY PFL_EXPORT_NAME_OF_COMPONMENTS)
  set(PFL_EXPORT_NAME_OF_COMPONMENTS
      "${PFL_EXPORT_NAME_OF_COMPONMENTS}\nset(EXPORT_NAME_OF_${TARGET_NAME} ${PFL_TARGET_EXPORT_NAME})"
  )
  set_property(GLOBAL PROPERTY PFL_EXPORT_NAME_OF_COMPONMENTS
                               ${PFL_EXPORT_NAME_OF_COMPONMENTS})

  install(
    TARGETS "${TARGET_FULL_NAME}"
    EXPORT "${PFL_TARGET_EXPORT_NAME}"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  _pfl_str_join(TARGET_FULL_PATH_DIR "/" ${_PFL_PATH})
  _pfl_str_join(TARGET_EXPORT_NAMESPACE "::" ${_PFL_PATH})

  set_target_properties("${TARGET_FULL_NAME}" PROPERTIES EXPORT_NAME
                                                         ${TARGET_NAME})

  install(
    EXPORT "${PFL_TARGET_EXPORT_NAME}"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_FULL_PATH_DIR}
    FILE "${PFL_TARGET_EXPORT_NAME}.cmake"
    NAMESPACE ${TARGET_EXPORT_NAMESPACE}::)

  set(CONFIG_FILE misc/cmake/${PFL_TARGET_EXPORT_NAME}Config.cmake)

  set(CONFIG_FILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}.in)
  if(NOT EXISTS ${CONFIG_FILE_IN})
    write_file(${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in
               ${PFL_add_library_default_cmake_config})
    set(CONFIG_FILE_IN ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in)
  endif()

  include(CMakePackageConfigHelpers)
  # This will be used to replace @PACKAGE_cmakeModulesDir@
  set(cmakeModulesDir cmake)

  set(PFL_FIND_DEPENDENCIES)
  foreach(ARGUMENTS ${PFL_ADD_LIBRARY_FIND_DEPENDENCY_ARGUMENTS})
    set(PFL_FIND_DEPENDENCIES
        "${PFL_FIND_DEPENDENCIES}find_dependency(${ARGUMENTS})\n")
  endforeach()
  configure_package_config_file(
    ${CONFIG_FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
    PATH_VARS cmakeModulesDir
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_FULL_PATH_DIR}
    NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  set(VERSION_FILE misc/cmake/${PFL_TARGET_EXPORT_NAME}ConfigVersion.cmake)
  write_basic_package_version_file(
    ${VERSION_FILE}
    VERSION ${PFL_ADD_LIBRARIES_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
                ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE}
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_FULL_PATH_DIR})
endfunction()

#[[

# PFL_add_excutable

This function is used to add an executable.
You should call this function
if `${CMAKE_CURRENT_SOURCE_DIR}/src` contains the source code
of your executable file.

For examples you could use this function to add applications, examples and test.

## Argument

- *string* `LIBEXEC`
  If this option is set,
  executable will be installed to `${CMAKE_INSTALL_LIBEXECDIR}/${LIBEXEC}` dir.
- *string* `OUTPUT_NAME`
  The output file name of your executable.
- *list* `LINK_LIBRARIES`
  Arguments passed to target_link_libraries.
- *list* `COMPILE_FEATURES`
  Arguments passed to target_compile_features.
- *list* `SOURCES`
  List of source files, include .in, sources and headers.
  We support add (possibly near-empty) cpp files
  as sources of header only library to check if that header is self-contained.
- *option* `DISABLE_INSTALL`

## Example

Check [examples](https://github.com/black-desk/PFL.cmake/tree/master/examples)
online.

]]
function(PFL_add_executable)
  cmake_parse_arguments(
    PFL_ADD_EXECUTABLE "DISABLE_INSTALL" "LIBEXEC;OUTPUT_NAME"
    "SOURCES;LINK_LIBRARIES;COMPILE_FEATURES" ${ARGN})

  _pfl_get_current_target_name(TARGET_NAME)
  set(OLD_PFL_PATH ${_PFL_PATH})
  list(APPEND _PFL_PATH ${TARGET_NAME})
  _pfl_str_join(TARGET_FULL_NAME "__" ${_PFL_PATH})
  _pfl_str_join(TARGET_FULL_ALIAS_NAME "::" ${_PFL_PATH})

  if(NOT PFL_ADD_EXECUTABLE_OUTPUT_NAME)
    set(PFL_ADD_EXECUTABLE_OUTPUT_NAME ${TARGET_NAME})

    _pfl_warn("OUTPUT_NAME of ${TARGET_FULL_ALIAS_NAME} not set,"
              "using ${PFL_ADD_EXECUTABLE_OUTPUT_NAME}")
  endif()

  _pfl_info("Adding executable ${PFL_ADD_EXECUTABLE_OUTPUT_NAME}"
            "as ${TARGET_FULL_ALIAS_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}")

  _pfl_handle_sources(HEADERS SOURCES OFF ${PFL_ADD_EXECUTABLE_SOURCES})

  add_executable(${TARGET_FULL_NAME})
  target_sources(${TARGET_FULL_NAME} PRIVATE ${SOURCES})

  if(NOT "${TARGET_FULL_ALIAS_NAME}" STREQUAL "${TARGET_FULL_NAME}")
    add_executable("${TARGET_FULL_ALIAS_NAME}" ALIAS "${TARGET_FULL_NAME}")
  endif()

  set_target_properties(
    "${TARGET_FULL_NAME}" PROPERTIES OUTPUT_NAME
                                     ${PFL_ADD_EXECUTABLE_OUTPUT_NAME})

  target_include_directories(
    ${TARGET_FULL_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/src)

  if(PFL_ADD_EXECUTABLE_LINK_LIBRARIES)
    target_link_libraries(${TARGET_FULL_NAME}
                          ${PFL_ADD_EXECUTABLE_LINK_LIBRARIES})
  endif()

  if(PFL_ADD_EXECUTABLE_COMPILE_FEATURES)
    target_compile_features(${TARGET_FULL_NAME}
                            ${PFL_ADD_EXECUTABLE_COMPILE_FEATURES})
  endif()

  if(PFL_ADD_EXECUTABLE_DISABLE_INSTALL OR NOT _PFL_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  if(PFL_ADD_EXECUTABLE_LIBEXEC)
    install(
      TARGETS ${TARGET_FULL_NAME}
      DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${PFL_ADD_EXECUTABLE_LIBEXEC})
    return()
  endif()

  install(TARGETS ${TARGET_FULL_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
