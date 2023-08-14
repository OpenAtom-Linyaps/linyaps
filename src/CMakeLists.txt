# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(LINGLONG_FILESET_BASEDIRS ${PROJECT_SOURCE_DIR}/src
                              ${PROJECT_BINARY_DIR}/src
)

function(linglong_private_library_add_headers target)
  target_sources(
    ${target}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${LINGLONG_FILESET_BASEDIRS}
           FILES
           ${ARGN}
  )
endfunction()

function(linglong_add_library target)
  set(LINGLONG_LIB_NAME ${LINGLONG_LIB_PREFIX}${target})
  set(LINGLONG_LIB_PREFIX ${LINGLONG_LIB_NAME}_)

  cmake_parse_arguments(
    LINGLONG_ADD_LIBRARY_PARSED
    ""
    "TYPE"
    "SUBDIRS;SOURCES;HEADERS;DEPENDENCIES"
    ${ARGN}
  )

  message("add_library ${LINGLONG_LIB_NAME}")

  add_library(
    ${LINGLONG_LIB_NAME} ${LINGLONG_ADD_LIBRARY_PARSED_TYPE}
                         ${LINGLONG_ADD_LIBRARY_PARSED_SOURECES}
  )
  target_sources(
    ${LINGLONG_LIB_NAME}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${LINGLONG_FILESET_BASEDIRS}
           FILES
           ${LINGLONG_ADD_LIBRARY_PARSED_HEADERS}
  )

  set(LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE INTERFACE)
  if(NOT
     DEFINED
     LINGLONG_ADD_LIBRARY_PARSED_TYPE
  )
    set(LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE PUBLIC)
  endif()

  foreach(LINGLONG_ADD_LIBRARY_SUBDIR ${LINGLONG_ADD_LIBRARY_PARSED_SUBDIRS})
    add_subdirectory(${LINGLONG_ADD_LIBRARY_SUBDIR})
    target_link_libraries(
      ${LINGLONG_LIB_NAME}
      ${LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE}
      "${LINGLONG_LIB_PREFIX}_${LINGLONG_SUB_DIRECTORY}"
    )
  endforeach()

  if(NOT LINGLONG_ADD_LIBRARY_PARSED_DEPENDENCIES)
    target_link_libraries(
      ${LINGLONG_LIB_NAME}
      ${LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE}
      ${LINGLONG_ADD_LIBRARY_PARSED_DEPENDENCIES}
    )
  endif()
endfunction()

add_subdirectory(dbusgen)

add_subdirectory(linglong)
