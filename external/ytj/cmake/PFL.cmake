# SPDX-FileCopyrightText: 2023 Chen Linxuan <me@black-desk.cn>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

get_property(
  PFL_INITIALIZED GLOBAL ""
  PROPERTY PFL_INITIALIZED
  SET)
if(PFL_INITIALIZED)
  return()
endif()

set_property(GLOBAL PROPERTY PFL_INITIALIZED true)

# You should call this function in your top level CMakeLists.txt to tell
# PFL.cmake some global settings about your project.
#
# ENABLE_TESTING bool: Whether to build tests for libraries or not.
#
# BUILD_EXAMPLES bool: Whether to build examples for libraries or not.
#
# EXTERNALS list of string: External projects to build.
function(pfl_init)
  cmake_parse_arguments(PFL_INIT "" "INSTALL;ENABLE_TESTING;BUILD_EXAMPLES"
                        "EXTERNALS" ${ARGN})

  message(STATUS "PFL: --==Version: v0.2.5==--")

  set(PFL_ENABLE_TESTING
      ${PFL_INIT_ENABLE_TESTING}
      PARENT_SCOPE)
  set(PFL_BUILD_EXAMPLES
      ${PFL_INIT_BUILD_EXAMPLES}
      PARENT_SCOPE)
  set(PFL_INSTALL
      ${PFL_INIT_INSTALL}
      PARENT_SCOPE)
  foreach(EXTERNAL ${PFL_INIT_EXTERNALS})
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/${EXTERNAL})
  endforeach()
endfunction()

function(__pfl_configure_files)
  cmake_parse_arguments(PFL_CONFIGURE_FILES "" "HEADERS;SOURCES" "INS" ${ARGN})

  foreach(IN_FILE ${PFL_CONFIGURE_FILES_INS})
    string(REGEX REPLACE "\.in$" "" OUT_FILE ${IN_FILE})

    configure_file(${IN_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE} @ONLY)

    if(OUT_FILE MATCHES "^(./)?include\/.*\.h(h|pp)?$")
      list(APPEND ${PFL_CONFIGURE_FILES_HEADERS}
           ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
    elseif(OUT_FILE MATCHES "^(./)?src\/.*\.(h(h|pp)?|cpp)$")
      list(APPEND ${PFL_CONFIGURE_FILES_SOURCES}
           ${CMAKE_CURRENT_BINARY_DIR}/${OUT_FILE})
    else()
      # TODO: add other files later
    endif()
  endforeach()

  set(${PFL_CONFIGURE_FILES_HEADERS}
      ${${PFL_CONFIGURE_FILES_HEADERS}}
      PARENT_SCOPE)

  set(${PFL_CONFIGURE_FILES_SOURCES}
      ${${PFL_CONFIGURE_FILES_SOURCES}}
      PARENT_SCOPE)
endfunction()

# This function is used to add libraries under the `libs` directory. It just
# takes in a string list contains the directory name under `libs`.
function(pfl_add_libraries)
  cmake_parse_arguments(PFL_ADD_LIBRARIES "" "VERSION" "LIBS" ${ARGN})

  message(
    STATUS
      "PFL:${PFL_MESSAGE_INDENT} Adding libraries at ${CMAKE_CURRENT_SOURCE_DIR}"
  )

  set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")

  cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME TARGET_DIR_NAME)

  if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    set(TARGET_DIR_NAME ${PROJECT_NAME})
  endif()

  string(REPLACE " " "_" TARGET_NAME "${TARGET_DIR_NAME}")

  if(PFL_PREFIX)
    set(PFL_PREFIX "${PFL_PREFIX}::${TARGET_NAME}")
  else()
    set(PFL_PREFIX "${TARGET_NAME}")
  endif()

  foreach(LIB ${PFL_ADD_LIBRARIES_LIBS})
    add_subdirectory(libs/${LIB})
  endforeach()

  if(NOT PFL_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  if(EXISTS
     ${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake/${TARGET_NAME}-config.cmake.in)
    include(CMakePackageConfigHelpers)
    # This will be used to replace @PACKAGE_cmakeModulesDir@
    set(cmakeModulesDir cmake)

    configure_package_config_file(
      misc/cmake/${TARGET_NAME}-config.cmake.in
      misc/cmake/${TARGET_NAME}-config.cmake
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
      PATH_VARS cmakeModulesDir
      NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

    write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config-version.cmake
      VERSION ${PFL_ADD_LIBRARIES_VERSION}
      COMPATIBILITY SameMajorVersion)

    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config-version.cmake
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME})
  endif()
endfunction()

# This function is used to add a library.
#
# HEADER_ONLY option: Whether this library is a header only library.
#
# INTERNAL option: Whether this library is a internal library.
#
# OUTPUT_NAME string: The output file name of this library.
#
# SOVERSION string: The soversion of this library.
#
# VERSION string: The version of this library.
#
# TYPE [STATIC | SHARED]: The type of this library.
#
# INS list of string: .in files to configure, it use @ONLY to configure_file.
#
# SOURCE list of string: file names of source and private headers.
#
# HEADERS list of string: file names of public headers.
#
# LINK_LIBRARIES: arguments passed to target_link_libraries.
#
# COMPILE_FEATURES: arguments target_compile_features.
#
# APPS: list of string: directory names under "apps" directory.
#
# EXAMPLES: list of string: directory names under "examples" directory.
function(pfl_add_library)
  cmake_parse_arguments(
    PFL_ADD_LIBRARY "HEADER_ONLY;INTERNAL" "OUTPUT_NAME;SOVERSION;VERSION;TYPE"
    "INS;SOURCES;HEADERS;LINK_LIBRARIES;COMPILE_FEATURES;APPS;EXAMPLES" ${ARGN})

  cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME TARGET_DIR_NAME)

  if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    set(TARGET_DIR_NAME ${PROJECT_NAME})
  endif()

  string(REPLACE " " "_" TARGET_NAME "${TARGET_DIR_NAME}")

  string(REPLACE "::" "__" TARGET_PREFIX "${PFL_PREFIX}")

  if(TARGET_PREFIX)
    set(TARGET_NAME "${TARGET_PREFIX}__${TARGET_NAME}")
  endif()

  string(REPLACE "__" "::" TARGET_EXPORT_NAME "${TARGET_NAME}")

  message(
    STATUS
      "PFL:${PFL_MESSAGE_INDENT} Adding library ${TARGET_EXPORT_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}"
  )
  set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")

  __pfl_configure_files(INS ${PFL_ADD_LIBRARY_INS} HEADERS
                        PFL_ADD_LIBRARY_HEADERS SOURCES PFL_ADD_LIBRARY_SOURCES)

  if(PFL_ADD_LIBRARY_HEADER_ONLY)
    add_library("${TARGET_NAME}" INTERFACE)
    add_library("${TARGET_NAME}__BUILD" ${PFL_ADD_LIBRARY_SOURCES})
    target_link_libraries("${TARGET_NAME}__BUILD" PUBLIC "${TARGET_NAME}")
  else()
    add_library("${TARGET_NAME}" ${PFL_ADD_LIBRARY_TYPE}
                                 ${PFL_ADD_LIBRARY_SOURCES})
  endif()

  if(PFL_ADD_LIBRARY_SOVERSION AND NOT PFL_ADD_LIBRARY_HEADER_ONLY)
    set_target_properties("${TARGET_NAME}"
                          PROPERTIES SOVERSION ${PFL_ADD_LIBRARY_SOVERSION})
  endif()

  if(NOT "${TARGET_EXPORT_NAME}" STREQUAL "${TARGET_NAME}")
    add_library("${TARGET_EXPORT_NAME}" ALIAS "${TARGET_NAME}")
  endif()

  set_target_properties("${TARGET_NAME}" PROPERTIES EXPORT_NAME
                                                    ${TARGET_DIR_NAME})

  if(PFL_ADD_LIBRARY_OUTPUT_NAME)
    set_target_properties("${TARGET_NAME}"
                          PROPERTIES OUTPUT_NAME ${PFL_ADD_LIBRARY_OUTPUT_NAME})
  endif()

  target_sources(
    ${TARGET_NAME}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           include
           ${CMAKE_CURRENT_BINARY_DIR}/include
           FILES
           ${PFL_ADD_LIBRARY_HEADERS})

  if(NOT PFL_ADD_LIBRARY_HEADER_ONLY)
    if(PFL_ADD_LIBRARY_INTERNAL)
      target_include_directories(${TARGET_NAME}
                                 PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/src)
    else()
      target_include_directories(${TARGET_NAME}
                                 PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/src)
    endif()
  endif()

  if(PFL_ADD_LIBRARY_LINK_LIBRARIES)
    target_link_libraries(${TARGET_NAME} ${PFL_ADD_LIBRARY_LINK_LIBRARIES})
  endif()

  if(PFL_ADD_LIBRARY_COMPILE_FEATURES)
    target_compile_features(${TARGET_NAME} ${PFL_ADD_LIBRARY_COMPILE_FEATURES})
  endif()

  if(PFL_INSTALL AND NOT PFL_ADD_LIBRARY_INTERNAL)
    include(GNUInstallDirs)

    install(
      TARGETS "${TARGET_NAME}"
      EXPORT "${TARGET_NAME}"
      FILE_SET HEADERS)

    install(
      EXPORT "${TARGET_NAME}"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
      FILE "${TARGET_NAME}.cmake"
      NAMESPACE ${PFL_PREFIX}::)

    if(EXISTS
       ${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake/${TARGET_NAME}-config.cmake.in)
      include(CMakePackageConfigHelpers)
      # This will be used to replace @PACKAGE_cmakeModulesDir@
      set(cmakeModulesDir cmake)

      configure_package_config_file(
        misc/cmake/${TARGET_NAME}-config.cmake.in
        misc/cmake/${TARGET_NAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_NAME}
        PATH_VARS cmakeModulesDir
        NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

      write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config-version.cmake
        VERSION ${PFL_ADD_LIBRARY_VERSION}
        COMPATIBILITY SameMajorVersion)

      install(
        FILES
          ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/misc/cmake/${TARGET_NAME}-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
    endif()
  endif()

  if(PFL_PREFIX)
    set(PFL_PREFIX "${PFL_PREFIX}::${TARGET_DIR_NAME}")
  else()
    set(PFL_PREFIX "${TARGET_DIR_NAME}")
  endif()

  if(PFL_ENABLE_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    message(
      STATUS
        "PFL:${PFL_MESSAGE_INDENT} Adding tests for library ${TARGET_EXPORT_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")
    add_subdirectory(tests)
    string(SUBSTRING "${PFL_MESSAGE_INDENT}" 2 -1 PFL_MESSAGE_INDENT)
  endif()

  if(PFL_ADD_LIBRARY_EXAMPLES
     AND PFL_BUILD_EXAMPLES
     AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    message(
      STATUS
        "PFL:${PFL_MESSAGE_INDENT} Adding examples for library ${TARGET_EXPORT_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}/examples"
    )
    set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")
    foreach(EXAMPLE ${PFL_ADD_LIBRARY_EXAMPLES})
      add_subdirectory("examples/${EXAMPLE}")
    endforeach()
    string(SUBSTRING "${PFL_MESSAGE_INDENT}" 2 -1 PFL_MESSAGE_INDENT)
  endif()

  if(PFL_ADD_LIBRARY_APPS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps")
    message(
      STATUS
        "PFL:${PFL_MESSAGE_INDENT} Adding apps for library ${TARGET_EXPORT_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}/apps"
    )
    set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")
    foreach(APP ${PFL_ADD_LIBRARY_APPS})
      add_subdirectory("apps/${APP}")
    endforeach()
    string(SUBSTRING "${PFL_MESSAGE_INDENT}" 2 -1 PFL_MESSAGE_INDENT)
  endif()
endfunction()

# This function is used to add executable, you should call this function if your
# source under src/ is the source code of your executable file. For examples:
# directories under apps, directories under examples or your project root if
# your project is not a library but a application.
#
# LIBEXEC option: Whether your executable is a application should be used by
# user directly or not. LIBEXEC executable will be installed to the LIBEXEC dir.
#
# INTERNAL option: Whether your executable is a internal executable. Internal
# executable will not be install at all.
#
# OUTPUT_NAME string: The output file name of your executable.
#
# Other parameters is just like pfl_add_library.
function(pfl_add_executable)
  cmake_parse_arguments(
    PFL_ADD_EXECUTABLE "LIBEXEC;INTERNAL" "OUTPUT_NAME;INSTALL_PREFIX"
    "INS;SOURCES;HEADERS;LINK_LIBRARIES;COMPILE_FEATURES" ${ARGN})

  cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME TARGET_DIR_NAME)

  string(REPLACE " " "_" TARGET_NAME "${TARGET_DIR_NAME}")

  string(REPLACE "::" "__" TARGET_PREFIX "${PFL_PREFIX}")

  if(TARGET_PREFIX)
    set(TARGET_NAME "${TARGET_PREFIX}__${TARGET_NAME}")
  endif()

  message(
    STATUS
      "PFL:${PFL_MESSAGE_INDENT} Adding executable ${TARGET_NAME} at ${CMAKE_CURRENT_SOURCE_DIR}"
  )
  set(PFL_MESSAGE_INDENT "${PFL_MESSAGE_INDENT}  ")

  __pfl_configure_files(
    INS ${PFL_ADD_EXECUTABLE_INS} HEADERS PFL_ADD_EXECUTABLE_HEADERS SOURCES
    PFL_ADD_EXECUTABLE_SOURCES)

  add_executable("${TARGET_NAME}" ${PFL_ADD_EXECUTABLE_SOURCES})

  if(PFL_ADD_EXECUTABLE_OUTPUT_NAME)
    set_target_properties(
      "${TARGET_NAME}" PROPERTIES OUTPUT_NAME ${PFL_ADD_EXECUTABLE_OUTPUT_NAME})
  endif()

  target_sources(
    ${TARGET_NAME}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           include
           ${CMAKE_CURRENT_BINARY_DIR}/include
           FILES
           ${PFL_ADD_EXECUTABLE_HEADERS})

  target_include_directories(${TARGET_NAME}
                             PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/src)

  if(PFL_ADD_EXECUTABLE_LINK_LIBRARIES)
    target_link_libraries(${TARGET_NAME} ${PFL_ADD_EXECUTABLE_LINK_LIBRARIES})
  endif()

  if(PFL_ADD_EXECUTABLE_COMPILE_FEATURES)
    target_compile_features(${TARGET_NAME}
                            ${PFL_ADD_EXECUTABLE_COMPILE_FEATURES})
  endif()

  if(PFL_ADD_EXECUTABLE_INTERNAL OR NOT PFL_INSTALL)
    return()
  endif()

  include(GNUInstallDirs)

  if(PFL_ADD_EXECUTABLE_LIBEXEC)
    install(
      TARGETS ${TARGET_NAME}
      DESTINATION
        ${CMAKE_INSTALL_LIBEXECDIR}/${PFL_ADD_EXECUTABLE_INSTALL_PREFIX})
    return()
  endif()

  install(
    TARGETS ${TARGET_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/${PFL_ADD_EXECUTABLE_INSTALL_PREFIX})

endfunction()
