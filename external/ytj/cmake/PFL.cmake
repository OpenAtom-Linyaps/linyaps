#[[
SPDX-FileCopyrightText: 2023 Chen Linxuan <me@black-desk.cn>
SPDX-License-Identifier: LGPL-3.0-or-later
]]

#[[

# PFL.cmake

This project is design to build c++ project follows [the Pitchfork layout].

You can just copy the [PFL.cmake] file into your awesome project,
then use like this:

```cmake
cmake_minimum_required(VERSION 3.11.4 FATAL_ERROR)

project (
  MyAwesomeProject
  VERSION      0.1.0
  HOMEPAGE_URL https://github.com/<your-username>/MyAwesomeProject
  LANGUAGES    CXX
               C
)

```

For futher information, check [examples] and [documents].

[PFL.cmake]: https://github.com/black-desk/PFL.cmake/releases/latest/download/PFL.cmake

[the Pitchfork layout]: https://github.com/vector-of-bool/pitchfork/blob/develop/data/spec.bs

[examples]: https://github.com/black-desk/PFL.cmake/tree/master/examples

[documents]: https://github.com/black-desk/PFL.cmake/tree/master/docs

]]

# NOTE: RHEL 8 use cmake 3.11.4
cmake_minimum_required(VERSION 3.11.4 FATAL_ERROR)

# Check whether PFL.cmake is added already
get_property(
  PFL_INITIALIZED GLOBAL
  PROPERTY PFL_INITIALIZED
  SET)
if(PFL_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY PFL_INITIALIZED true)

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

function(_pfl_debug)
  if(CMAKE_VERSION VERSION_LESS 3.15)
    return()
  endif()
  _pfl_message(DEBUG ${ARGV})
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

set(_PFL_VERSION "v0.5.1")

_pfl_info("Version: ${_PFL_VERSION}")

set(_PFL_COMPONENT_VALIDATOR "^(_.*|.*__.*|.*_|.*:.*)\$")
macro(_pfl_check_component COMPONENT) # MESSAGES
  if("${COMPONENT}" MATCHES "${_PFL_COMPONENT_VALIDATOR}")
    _pfl_fatal(${ARGN})
  endif()
endmacro()

macro(_pfl_project_is_top_level)
  if(CMAKE_VERSION VERSION_LESS 3.21)
    # https://www.scivision.dev/cmake-project-is-top-level/
    get_property(
      not_top
      DIRECTORY
      PROPERTY PARENT_DIRECTORY)
    if(NOT not_top)
      set(PROJECT_IS_TOP_LEVEL true)
    else()
      set(PROJECT_IS_TOP_LEVEL false)
    endif()
  endif()
endmacro()

macro(_pfl_parse_arguments)
  foreach(PARAMETER IN LISTS FLAGS ONE_VALUE_KEYWORDS MULTI_VALUE_KEYWORDS)
    unset(PFL_ARG_${PARAMETER})
  endforeach()
  cmake_parse_arguments(PFL_ARG "${FLAGS}" "${ONE_VALUE_KEYWORDS}"
                        "${MULTI_VALUE_KEYWORDS}" ${ARGN})
endmacro()

function(_pfl_check_called_from_project_source_dir FUNCTION_NAME)
  if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    return()
  endif()

  _pfl_fatal("${FUNCTION_NAME} must be called at \${PROJECT_SOURCE_DIR}")
endfunction()

function(_pfl_check_project_name)
  _pfl_check_component(
    "${PROJECT_NAME}" "Unsupported project name \"${PROJECT_NAME}\" detected."
    "Please rename your project.")
endfunction()

macro(PFL_init) # NOTE: As we might call enable_testing() in `PFL_init`, it have
                # to be a macro. Otherwise enable_testing() will not take
                # effect.
  set(FLAGS
      # Do not automatically detected parameters from defined variables.
      # NOTE:
      # It's not recommended to disable auto detected.
      NO_AUTO)
  set(ONE_VALUE_KEYWORDS
      # Make `PFL_add_library()` to default to `SHARED` libraries.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      BUILD_SHARED_LIBS # boolean
      # Enable `install` target of this project.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      ENABLE_INSTALL # boolean
      # Enable CTest and compile test executables.
      # Check comments of `TESTS` parameters
      # in PFL_add_library() and PFL_add_libraries() for details.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      ENABLE_TESTING # boolean
      # Compile example executables.
      # Check comments of `EXAMPLES` parameters
      # in PFL_add_library() and PFL_add_libraries() for details.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      ENABLE_EXAMPLES # boolean
      # Compile embed third party projects.
      # Check comments of `EXTERNALS` parameters for details.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      ENABLE_EXTERNALS # boolean
      # Compile applications.
      # Check comments of `APPS` parameters
      # in PFL_add_library() and PFL_add_libraries() for details.
      # [ default: ${PROJECT_IS_TOP_LEVEL} ]
      ENABLE_APPLICATIONS # boolean
  )
  set(MULTI_VALUE_KEYWORDS
      # List of third party projects which current project depends on.
      # Each string in this list indicate a external project in this syntax:
      # "DIRECTORY PACKAGE"
      # It means that the external project
      # in ${CMAKE_CURRENT_SOURCE_DIR}/external/<DIRECTORY>
      # should override find_package(<PACKAGE>),
      # when `ENABLE_EXTERNALS` is true.
      EXTERNALS)
  _pfl_parse_arguments(${ARGN})

  _pfl_project_is_top_level()

  _pfl_check_called_from_project_source_dir(PFL_init)
  _pfl_check_project_name()

  set(auto true)
  if(PFL_ARG_NO_AUTO)
    set(auto false)
  endif()

  if(auto)
    macro(_pfl_init_produce_auto_argument NAME DEFAULT)
      if(DEFINED ${PROJECT_NAME}_${NAME} AND DEFINED PLF_ARG_${NAME})
        _pfl_fatal(
          "You may either define ${PROJECT_NAME}_${NAME} before call PFL_INIT"
          "or call PFL_INIT with argument ${NAME}.")
      endif()

      if(NOT DEFINED ${PROJECT_NAME}_${NAME})
        set("${PROJECT_NAME}_${NAME}" ${DEFAULT})
      endif()

      if(DEFINED PFL_ARG_${NAME})
        set("${PROJECT_NAME}_${NAME}" ${PFL_ARG_${NAME}})
      endif()
    endmacro()

    foreach(ARG IN LISTS FLAGS ONE_VALUE_KEYWORDS MULTI_VALUE_KEYWORDS)
      if(ARG STREQUAL NO_AUTO)
        continue()
      endif()
      if(ARG STREQUAL EXTERNALS)
        _pfl_init_produce_auto_argument(EXTERNALS "")
        continue()
      endif()
      _pfl_init_produce_auto_argument(${ARG} ${PROJECT_IS_TOP_LEVEL})
    endforeach()
  endif()

  set(_PFL_PATH)
  list(APPEND _PFL_PATH ${PROJECT_NAME})

  if(${PROJECT_NAME}_ENABLE_TESTING)
    enable_testing()
  endif()

  macro(generate_find_for_external PACKAGE TARGET)
    # message(FATAL_ERROR "${PACKAGE} ${TARGET}")
    if(NOT EXISTS
       ${CMAKE_CURRENT_SOURCE_DIR}/external/${PACKAGE}/CMakeLists.txt)
      _pfl_fatal(
        "${DIRECTORY} is not a external project:"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/${DIRECTORY}/CMakeLists.txt not found."
      )
    endif()

    set(FIND_FILE
        ${CMAKE_CURRENT_BINARY_DIR}/_pfl_external/Find${PACKAGE}.cmake)
    file(
      WRITE ${FIND_FILE}
      "if(TARGET ${TARGET})\n" #
      "  return()\n" #
      "endif()\n" #
      "add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/${PACKAGE})\n"
      "set(${PACKAGE}_FOUND YES)")
  endmacro()

  if(${PROJECT_NAME}_ENABLE_EXTERNALS AND DEFINED ${PROJECT_NAME}_EXTERNALS)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR}/_pfl_external)
    foreach(EXTERNAL ${${PROJECT_NAME}_EXTERNALS})
      string(REPLACE " " ";" EXTERNAL "${EXTERNAL}")
      list(GET EXTERNAL 0 PACKAGE_NAME)
      _pfl_info(
        "Using external project ${PACKAGE_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}/external/${PACKAGE_NAME}"
      )
      generate_find_for_external(${EXTERNAL})
      find_package(${PACKAGE_NAME} REQUIRED)
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

@PFL_EXPORT_FILE_NAME_OF_COMPONMENTS@

# Check all required components are available before trying to load any
foreach(Component ${Components})
  if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_${Component})
    continue();
  endif()

  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/${EXPORT_FILE_NAME_OF_${Component}}.cmake)
    continue();
  endif()

  set(${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE
      "Missing required component: ${Component}")

  set(${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)
  return()
endforeach()

foreach(Component ${Components})
  include(${CMAKE_CURRENT_LIST_DIR}/${EXPORT_FILE_NAME_OF_${Component}}.cmake OPTIONAL)
endforeach()
]=])

function(PFL_add_libraries)

  set(FLAGS)
  set(ONE_VALUE_KEYWORDS)
  set(MULTI_VALUE_KEYWORDS
      # Directories under ${CMAKE_CURRENT_SOURCE_DIR}/libs,
      # that should be treated as submodule or components of your project.
      # These projects will be added one by one using add_subdirectory().
      LIBS
      # Same as EXAMPLES of PFL_add_library
      EXAMPLES
      # Same as APPS of PFL_add_library
      APPS
      # Same as TESTS of PFL_add_library
      TESTS)
  _pfl_check_called_from_project_source_dir(PFL_add_libraries)
  _pfl_parse_arguments(${ARGN})

  _pfl_info("Adding libraries at ${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT DEFINED PFL_ARG_LIBS)
    _pfl_fatal("LIBS is required.")
  endif()

  if(NOT PFL_ARG_LIBS)
    _pfl_fatal("At least one LIBS is required.")
  endif()

  set_property(GLOBAL PROPERTY PFL_COMPONMENTS)
  set_property(GLOBAL PROPERTY PFL_EXPORT_FILE_NAME_OF_COMPONMENTS)
  foreach(LIB ${PFL_ARG_LIBS})
    add_subdirectory(libs/${LIB})
  endforeach()

  _pfl_add_executables()

  if(NOT ${PROJECT_NAME}_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  set(CONFIG_FILE misc/cmake/${PROJECT_NAME}Config.cmake)

  set(CONFIG_FILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}.in)
  if(NOT EXISTS ${CONFIG_FILE_IN})
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in
         ${PFL_add_libraries_default_cmake_config})
    set(CONFIG_FILE_IN ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in)
  endif()

  include(CMakePackageConfigHelpers)
  # This will be used to replace @PACKAGE_cmakeModulesDir@
  set(cmakeModulesDir cmake)

  get_property(PFL_COMPONMENTS GLOBAL PROPERTY PFL_COMPONMENTS)
  get_property(PFL_EXPORT_FILE_NAME_OF_COMPONMENTS GLOBAL
               PROPERTY PFL_EXPORT_FILE_NAME_OF_COMPONMENTS)
  configure_package_config_file(
    ${CONFIG_FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
    PATH_VARS cmakeModulesDir
    INSTALL_DESTINATION
      ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
    NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  set(VERSION_FILE misc/cmake/${PROJECT_NAME}ConfigVersion.cmake)
  write_basic_package_version_file(
    ${VERSION_FILE}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
          ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )
endfunction()

function(_pfl_add_tests)
  foreach(TEST ${ARGN})
    if(${PROJECT_NAME}_ENABLE_TESTING)
      include(CTest)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST})
    else()
      _pfl_info("Skip test: ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST}")
    endif()
  endforeach()
endfunction()

function(_pfl_add_examples)
  foreach(EXAMPLE ${ARGV})
    if(${PROJECT_NAME}_ENABLE_EXAMPLES)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples/${EXAMPLE})
    else()
      _pfl_info("Skip example: ${CMAKE_CURRENT_SOURCE_DIR}/examples/${EXAMPLE}")
    endif()
  endforeach()
endfunction()

function(_pfl_add_apps)
  foreach(APP ${ARGV})
    if(${PROJECT_NAME}_ENABLE_APPLICATIONS)
      add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/apps/${APP})
    else()
      _pfl_info("Skip app: ${CMAKE_CURRENT_SOURCE_DIR}/apps/${APP}")
    endif()
  endforeach()
endfunction()

macro(_pfl_current_compoent)
  set(PFL_CURRENT_COMPOENT ${PROJECT_NAME})

  if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    get_filename_component(PFL_CURRENT_COMPOENT ${CMAKE_CURRENT_SOURCE_DIR}
                           NAME)
    _pfl_check_component(
      "${PFL_CURRENT_COMPOENT}"
      "Unsupported directory name \"${PFL_CURRENT_COMPOENT}\" detected."
      "Please rename your directory.")
  endif()
endmacro()

function(_pfl_handle_sources PUBLIC_OUT PRIVATE_OUT MERGED_PLACEMENT) # SOURCES
  set(SOURCES ${ARGN})

  set(PUBLIC_SOURCES)
  set(PRIVATE_SOURCES)

  foreach(SOURCE ${SOURCES})
    if(SOURCE MATCHES "^\./")
      string(REGEX REPLACE "^\./" "" SOURCE ${SOURCE})
    endif()

    if(SOURCE MATCHES ".*\\.\\./.*")
      _pfl_fatal("${SOURCE} is invalid:"
                 "../ is not allowed when add source to target.")
    endif()

    set(FULL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE})

    if(SOURCE MATCHES "\.in$")
      string(REGEX REPLACE "\.in$" "" OUT_FILE ${SOURCE})
      configure_file("${SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}"
                     @ONLY)
      set(SOURCE "${OUT_FILE}")
      set(FULL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE}")
    endif()

    set(PUBLIC_REGEXP "include/.*\$")
    if(MERGED_PLACEMENT)
      set(PUBLIC_REGEXP "src\/.*\.h(h|pp)?\$")
    endif()

    if(NOT EXISTS "${FULL_SOURCE}")
      _pfl_fatal("source file do not found at ${FULL_SOURCE}")
    endif()

    if(SOURCE MATCHES "${PUBLIC_REGEXP}")
      set(TARGET_LIST PUBLIC_SOURCES)
    else()
      set(TARGET_LIST PRIVATE_SOURCES)
    endif()

    list(APPEND ${TARGET_LIST} ${FULL_SOURCE})
  endforeach()

  set("${PUBLIC_OUT}"
      ${PUBLIC_SOURCES}
      PARENT_SCOPE)
  set("${PRIVATE_OUT}"
      ${PRIVATE_SOURCES}
      PARENT_SCOPE)
endfunction()

macro(_pfl_add_target_common)
  if(DEFINED PFL_ARG_LINK_LIBRARIES)
    target_link_libraries(${TARGET} ${PFL_ARG_LINK_LIBRARIES})
  endif()

  if(DEFINED PFL_ARG_COMPILE_FEATURES)
    target_compile_features(${TARGET} ${PFL_ARG_COMPILE_FEATURES})
  endif()

  if(DEFINED PFL_ARG_COMPILE_OPTIONS)
    target_compile_options(${TARGET} ${PFL_ARG_COMPILE_OPTIONS})
  endif()

  if(DEFINED PFL_ARG_DEPENDENCIES)
    foreach(DEPENDENCY ${PFL_ARG_DEPENDENCIES})
      if("${DEPENDENCY}" STREQUAL "PUBLIC" OR "${DEPENDENCY}" STREQUAL
                                              "PRIVATE")
        continue()
      endif()

      string(REPLACE " " ";" DEPENDENCY ${DEPENDENCY})
      find_package(${DEPENDENCY})
    endforeach()
  endif()
endmacro()

macro(_pfl_add_executables)
  if(DEFINED PFL_ARG_TESTS)
    _pfl_add_tests(${PFL_ARG_TESTS})
  endif()

  if(DEFINED PFL_ARG_EXAMPLES)
    _pfl_add_examples(${PFL_ARG_EXAMPLES})
  endif()

  if(DEFINED PFL_ARG_APPS)
    _pfl_add_apps(${PFL_ARG_APPS})
  endif()
endmacro()

set(PFL_add_library_default_cmake_config
    [=[
@PACKAGE_INIT@

list(APPEND CMAKE_MODULE_PATH "@PACKAGE_cmakeModulesDir@")

include(CMakeFindDependencyMacro)


@PFL_FIND_DEPENDENCIES@

include(${CMAKE_CURRENT_LIST_DIR}/@PFL_EXPORT_FILE_NAME@.cmake)
]=])

function(pfl_add_library)
  set(FLAGS
      # Disable install target of this library.
      # If install target of this library is enabled,
      # PFL.cmake will export your library automatically as well.
      # [ default: FALSE ]
      # NOTE: It's recommend to set this to true for internal libraries.
      DISABLE_INSTALL
      # Use merged header placement.
      # This make PFL.cmake treats all header files under src as public headers.
      # [ default: FALSE ]
      MERGED_HEADER_PLACEMENT)
  set(ONE_VALUE_KEYWORDS
      # Set the type of this library.
      # Valid options are `STATIC`, `SHARED` or `HEADER_ONLY`.
      # [ default: BUILD_SHARED_LIBS ? `SHARED` : `STATIC` ]
      LIBRARY_TYPE # string
      # The output file name of this library.
      # [ default: All the PFL hierarchies joined with "__" ]
      # NOTE: It's not recommend to use the default value.
      OUTPUT_NAME # string
      # The version of this library.
      # [ default: ${PROJECT_VERSION} ]
      VERSION # string
      # The soversion of this library.
      # [ defualt: ${VERSION} ]
      # NOTE:
      # 1. This option is not available for `HEADER_ONLY` and `STATIC` library.
      # 2. CMake defaults to keep SOVERSION same as VERSION.
      SOVERSION # string
  )
  set(MULTI_VALUE_KEYWORDS
      # List of relative file paths from ${CMAKE_CURRENT_SOURCE_DIR},
      # including input files (.in), sources (.c .cpp .cxx .cc)
      # and headers (.h .hpp .hh).
      # Files without extension treated as headers also.
      # Input files will be configured with @ONLY mode of configure_file.
      # And the output is added to library as sources as well.
      # By default, files under ${CMAKE_CURRENT_SOURCE_DIR}/include is public,
      # while all other files is private.
      # If MERGED_HEADER_PLACEMENT is true,
      # header files under ${CMAKE_CURRENT_SOURCE_DIR}/src is public as well.
      # [ default: All files under ${CMAKE_CURRENT_SOURCE_DIR}/src
      #            and ${CMAKE_CURRENT_SOURCE_DIR}/include
      # ]
      # NOTE:
      # We support add (possibly near-empty) cpp files
      # as sources for header only library.
      # And PFL.cmake will build an internal static library that
      # PRIVATE link to the header only library
      # to check if all header file is self-contained.
      SOURCES
      # List of relative paths to
      # directories in ${CMAKE_CURRENT_SOURCE_DIR}/tests
      # which contains source code of tests.
      # If test targets is enabled in your project,
      # PFL.cmake will call add_subdirectory on these directory in order.
      # These directories were expected to have a CMakeLists.txt,
      # which call pfl_add_executable().
      TESTS
      # List of relative paths to
      # directories in ${CMAKE_CURRENT_SOURCE_DIR}/apps
      # which contain source code of executables.
      # If application targets is enabled in your project,
      # PFL.cmake will call add_subdirectory on these directory in order.
      # These directories were expected to have a CMakeLists.txt,
      # which call pfl_add_executable().
      APPS
      # List of relative paths to
      # directories in ${CMAKE_CURRENT_SOURCE_DIR}/examples
      # which contain source code of executables.
      # If example targets is enabled in your project,
      # PFL.cmake will call add_subdirectory on these directory in order.
      # These directories were expected to have a CMakeLists.txt,
      # which call pfl_add_executable().
      EXAMPLES
      # Arguments pass to find_package()
      # to make sure your dependencies is founded.
      # pfl_add_library (
      #   ...
      #   DEPENDENCIES
      #     PUBLIC "aaa COMPONENTS xxx REQUIRED"
      #     PRIVATE "bbb REQUIRED"
      #   ...
      # )
      # PRIVATE while building and
      # find_dependency() in auto-generated `*Config.cmake` file.
      # NOTE:
      # PFL.cmake is going to use these arguments to:
      # 1. Call find_package for your before build your library.
      # 2. Call find_dependency in the export file of your library,
      #    if install targets of your library is enabled.
      DEPENDENCIES
      # Arguments passed to target_link_libraries().
      LINK_LIBRARIES
      # Arguments passed to target_compile_features().
      COMPILE_FEATURES
      # Arguments passed to target_compile_options().
      COMPILE_OPTIONS)
  _pfl_parse_arguments(${ARGN})
  _pfl_current_compoent()

  set(OLD_PFL_PATH ${_PFL_PATH})
  list(APPEND _PFL_PATH ${PFL_CURRENT_COMPOENT})
  _pfl_str_join(TARGET "__" ${_PFL_PATH})
  _pfl_str_join(TARGET_ALIAS "::" ${_PFL_PATH})

  _pfl_info("Adding library ${TARGET_ALIAS} at ${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT DEFINED PFL_ARG_MERGED_HEADER_PLACEMENT)
    set(PFL_ARG_MERGED_HEADER_PLACEMENT false)
  endif()

  if((NOT DEFINED PFL_ARG_SOURCES) OR (NOT PFL_ARG_SOURCES))
    _pfl_fatal("SOURCES is missing."
               "Pure interface libraries is not supported now.")
  endif()

  _pfl_handle_sources(PUBLIC_SOURCES PRIVATE_SOURCES
                      ${PFL_ARG_MERGED_HEADER_PLACEMENT} ${PFL_ARG_SOURCES})

  if(NOT DEFINED PFL_ARG_LIBRARY_TYPE)
    set(PFL_ARG_LIBRARY_TYPE "STATIC")
    if(${PROJECT_NAME}_BUILD_SHARED_LIBS)
      set(PFL_ARG_LIBRARY_TYPE "SHARED")
    endif()
  endif()

  set(LIBRARY_TYPE ${PFL_ARG_LIBRARY_TYPE})
  set(PUBLIC_SOURCES_PROPAGATE PUBLIC)
  if("${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    set(LIBRARY_TYPE INTERFACE)
    set(PUBLIC_SOURCES_PROPAGATE INTERFACE)
  endif()

  add_library(${TARGET} ${LIBRARY_TYPE})
  if(NOT "${TARGET_ALIAS}" STREQUAL "${TARGET}")
    add_library("${TARGET_ALIAS}" ALIAS "${TARGET}")
  endif()

  foreach(PUBLIC_SOURCE ${PUBLIC_SOURCES})
    target_sources(${TARGET} ${PUBLIC_SOURCES_PROPAGATE}
                             $<BUILD_INTERFACE:${PUBLIC_SOURCE}>)
  endforeach()

  set(PUBLIC_INCLUDE_PREFIX include)
  if(PFL_ARG_MERGED_HEADER_PLACEMENT)
    set(PUBLIC_INCLUDE_PREFIX src)
  endif()

  target_include_directories(
    ${TARGET}
    ${PUBLIC_SOURCES_PROPAGATE}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${PUBLIC_INCLUDE_PREFIX}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/${PUBLIC_INCLUDE_PREFIX}>)

  set(BUILD_LIBRARY_NAME ${TARGET})
  if("${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    set(BUILD_LIBRARY_NAME ${TARGET}__BUILD)
    add_library(${BUILD_LIBRARY_NAME} STATIC)
    target_link_libraries(${BUILD_LIBRARY_NAME} PRIVATE ${TARGET})
  endif()

  target_sources(${BUILD_LIBRARY_NAME} PRIVATE ${PRIVATE_SOURCES})

  target_include_directories(
    ${BUILD_LIBRARY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                                  ${CMAKE_CURRENT_BINARY_DIR}/src)

  if(DEFINED PFL_ARG_SOVERSION)
    if("${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
      _pfl_fatal(
        "SOVERSION is not available for HEADER_ONLY library ${TARGET_ALIAS}.")
    endif()

    set_target_properties("${TARGET}" PROPERTIES SOVERSION ${PFL_ARG_SOVERSION})
  endif()

  if(DEFINED PFL_ARG_VERSION)
    if("${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
      _pfl_fatal(
        "VERSION is not available for HEADER_ONLY library ${TARGET_ALIAS}.")
    endif()
  endif()

  if(NOT DEFINED PFL_ARG_VERSION)
    set(PFL_ARG_VERSION ${PROJECT_VERSION})
  endif()

  if(NOT "${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    set_target_properties("${TARGET}" PROPERTIES VERSION ${PFL_ARG_VERSION})
  endif()

  if(DEFINED PFL_ARG_OUTPUT_NAME AND "${PFL_ARG_LIBRARY_TYPE}" STREQUAL
                                     "HEADER_ONLY")
    _pfl_fatal("OUTPUT_NAME is not available"
               "for HEADER_ONLY library ${TARGET_ALIAS}.")
  endif()

  if(NOT DEFINED PFL_ARG_OUTPUT_NAME)
    set(PFL_ARG_OUTPUT_NAME "${TARGET}")
    if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
      set(PFL_ARG_OUTPUT_NAME ${PFL_CURRENT_COMPOENT})
    endif()
  endif()

  if(NOT "${PFL_ARG_LIBRARY_TYPE}" STREQUAL "HEADER_ONLY")
    _pfl_warn("OUTPUT_NAME of ${TARGET_ALIAS} not set,"
              "using ${PFL_ARG_OUTPUT_NAME}")
    set_target_properties("${TARGET}" PROPERTIES OUTPUT_NAME
                                                 ${PFL_ARG_OUTPUT_NAME})
  endif()

  _pfl_add_target_common()
  _pfl_add_executables()

  set(_PFL_PATH ${OLD_PFL_PATH})

  if(PFL_ARG_DISABLE_INSTALL OR NOT ${PROJECT_NAME}_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)
  set(PROJECT_PREFIX ${PROJECT_NAME}-${PROJECT_VERSION})
  set(COMPONENT_INCLUDE_SUBDIR ${PROJECT_PREFIX}/${PFL_CURRENT_COMPOENT})

  foreach(FILE ${PUBLIC_SOURCES})
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
      DESTINATION
        ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${COMPONENT_INCLUDE_SUBDIR}/${DIR})

    target_sources(
      "${TARGET}"
      ${PUBLIC_SOURCES_PROPAGATE}
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${COMPONENT_INCLUDE_SUBDIR}/${RELATIVE_FILE}>
    )
  endforeach()

  target_include_directories(
    ${TARGET}
    ${PUBLIC_SOURCES_PROPAGATE}
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${COMPONENT_INCLUDE_SUBDIR}>
  )

  set(PFL_EXPORT_FILE_NAME ${PFL_ARG_OUTPUT_NAME})

  get_property(PFL_COMPONMENTS GLOBAL PROPERTY PFL_COMPONMENTS)
  list(APPEND PFL_COMPONMENTS ${PFL_CURRENT_COMPOENT})
  set_property(GLOBAL PROPERTY PFL_COMPONMENTS ${PFL_COMPONMENTS})

  get_property(PFL_EXPORT_FILE_NAME_OF_COMPONMENTS GLOBAL
               PROPERTY PFL_EXPORT_FILE_NAME_OF_COMPONMENTS)
  set(PFL_EXPORT_FILE_NAME_OF_COMPONMENTS
      "${PFL_EXPORT_FILE_NAME_OF_COMPONMENTS}\nset(EXPORT_FILE_NAME_OF_${PFL_CURRENT_COMPOENT} ${PFL_EXPORT_FILE_NAME})"
  )
  set_property(GLOBAL PROPERTY PFL_EXPORT_FILE_NAME_OF_COMPONMENTS
                               ${PFL_EXPORT_FILE_NAME_OF_COMPONMENTS})

  install(
    TARGETS "${TARGET}"
    EXPORT "${PFL_ARG_OUTPUT_NAME}"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

  set_target_properties("${TARGET}" PROPERTIES EXPORT_NAME
                                               ${PFL_CURRENT_COMPOENT})

  set(INSTALL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_PREFIX})

  install(
    EXPORT "${PFL_ARG_OUTPUT_NAME}"
    DESTINATION ${INSTALL_CMAKE_DIR}
    FILE "${PFL_EXPORT_FILE_NAME}.cmake"
    NAMESPACE ${PROJECT_NAME}::)

  set(CONFIG_FILE misc/cmake/${PFL_ARG_OUTPUT_NAME}Config.cmake)

  set(CONFIG_FILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_FILE}.in)
  if(NOT EXISTS ${CONFIG_FILE_IN})
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in
         ${PFL_add_library_default_cmake_config})
    set(CONFIG_FILE_IN ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}.in)
  endif()

  include(CMakePackageConfigHelpers)
  # This will be used to replace @PACKAGE_cmakeModulesDir@
  set(cmakeModulesDir cmake)

  set(PFL_FIND_DEPENDENCIES)
  set(PROPAGATE PRIVATE)
  foreach(DEPENDENCY ${PFL_ARG_DEPENDENCIES})
    if("${DEPENDENCY}" STREQUAL "PUBLIC")
      set(PROPAGATE PUBLIC)
      continue()
    elseif("${DEPENDENCY}" STREQUAL "PRIVATE")
      set(PROPAGATE PRIVATE)
      continue()
    endif()

    if("${PROPAGATE}" STREQUAL "PRIVATE")
      _pfl_warn("Skip private dependency: ${DEPENDENCY}")
      continue()
    endif()

    set(PFL_FIND_DEPENDENCIES
        "${PFL_FIND_DEPENDENCIES}find_dependency(${DEPENDENCY})\n")
  endforeach()
  configure_package_config_file(
    ${CONFIG_FILE_IN} ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
    PATH_VARS cmakeModulesDir
    INSTALL_DESTINATION ${INSTALL_CMAKE_DIR}
    NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  set(VERSION_FILE misc/cmake/${PFL_ARG_OUTPUT_NAME}ConfigVersion.cmake)
  write_basic_package_version_file(
    ${VERSION_FILE}
    VERSION ${PFL_ARG_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILE}
                ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE}
          DESTINATION ${INSTALL_CMAKE_DIR})
endfunction()

function(PFL_add_executable)
  set(FLAGS #
      # Disable install target of this executable.
      # NOTE: It's recommend to set this for tests and examples.
      DISABLE_INSTALL)
  set(ONE_VALUE_KEYWORDS
      # Install this executable in ${CMAKE_INSTALL_LIBEXECDIR}/${LIBEXEC},
      # instand of ${CMAKE_INSTALL_BINDIR}.
      LIBEXEC # string
      # The output file name of this executable.
      # [ default: All the PFL hierarchies joined with "__" ]
      # NOTE: It's not recommend to use the default value.
      OUTPUT_NAME # string
  )
  set(MULTI_VALUE_KEYWORDS
      # List of relative file paths from ${CMAKE_CURRENT_SOURCE_DIR},
      # including input files (.in), sources (.c .cpp .cxx .cc)
      # and headers (.h .hpp .hh).
      # Files without extension treated as headers also.
      # Input files will be configured with @ONLY mode of configure_file.
      # And the output is added to library as sources as well.
      # [ default: All files under ${CMAKE_CURRENT_SOURCE_DIR}/src
      #            and ${CMAKE_CURRENT_SOURCE_DIR}/include ]
      SOURCES
      # Arguments pass to find_package() before build your executable.
      DEPENDENCIES
      # Arguments passed to target_link_libraries().
      LINK_LIBRARIES
      # Arguments passed to target_compile_features().
      COMPILE_FEATURES
      # Arguments passed to target_compile_options().
      COMPILE_OPTIONS)
  _pfl_parse_arguments(${ARGN})
  _pfl_current_compoent()

  set(OLD_PFL_PATH ${_PFL_PATH})
  list(APPEND _PFL_PATH ${PFL_CURRENT_COMPOENT})

  _pfl_str_join(TARGET "__" ${_PFL_PATH})
  _pfl_str_join(TARGET_ALIAS "::" ${_PFL_PATH})

  if(NOT DEFINED PFL_ARG_OUTPUT_NAME)
    set(PFL_ARG_OUTPUT_NAME ${PFL_CURRENT_COMPOENT})

    _pfl_warn("OUTPUT_NAME of ${TARGET_ALIAS} not set,"
              "using ${PFL_ARG_OUTPUT_NAME}")
  endif()

  _pfl_info("Adding executable ${PFL_ARG_OUTPUT_NAME}"
            "as ${TARGET_ALIAS} at ${CMAKE_CURRENT_SOURCE_DIR}")

  add_executable(${TARGET})
  target_sources(${TARGET} PRIVATE ${PFL_ARG_SOURCES})

  if(NOT "${TARGET_ALIAS}" STREQUAL "${TARGET}")
    add_executable("${TARGET_ALIAS}" ALIAS "${TARGET}")
  endif()

  set_target_properties("${TARGET}" PROPERTIES OUTPUT_NAME
                                               ${PFL_ARG_OUTPUT_NAME})

  target_include_directories(
    ${TARGET}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/src)

  _pfl_add_target_common()

  if(PFL_ARG_DISABLE_INSTALL OR NOT ${PROJECT_NAME}_ENABLE_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  if(DEFINED PFL_ARG_LIBEXEC)
    install(TARGETS ${TARGET}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${PFL_ARG_LIBEXEC})
    return()
  endif()

  install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
