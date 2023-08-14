# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(LINGLONG_FILESET_BASEDIRS ${PROJECT_SOURCE_DIR}/src
                              ${PROJECT_BINARY_DIR}/src
)

function(linglong_add_library target)
  string(
    REPLACE "_"
            ""
            target
            ${target}
  )
  set(LINGLONG_LIB_NAME ${LINGLONG_LIB_PREFIX}${target})
  set(LINGLONG_LIB_PREFIX ${LINGLONG_LIB_NAME}_)

  cmake_parse_arguments(
    LINGLONG_ADD_LIBRARY_PARSED
    ""
    ""
    "SUBDIRS;SOURCES;HEADERS;DEPENDENCIES"
    ${ARGN}
  )

  message(STATUS "adding library ${LINGLONG_LIB_NAME}")

  if(NOT LINGLONG_ADD_LIBRARY_PARSED_SOURCES)
    add_library(${LINGLONG_LIB_NAME} INTERFACE)
    set(LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE INTERFACE)
  else()
    add_library(${LINGLONG_LIB_NAME} ${LINGLONG_ADD_LIBRARY_PARSED_SOURCES})
    set(LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE PUBLIC)
  endif()

  target_sources(
    ${LINGLONG_LIB_NAME}
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${LINGLONG_FILESET_BASEDIRS}
           FILES
           ${LINGLONG_ADD_LIBRARY_PARSED_HEADERS}
  )

  foreach(LINGLONG_ADD_LIBRARY_SUBDIR ${LINGLONG_ADD_LIBRARY_PARSED_SUBDIRS})
    add_subdirectory(${LINGLONG_ADD_LIBRARY_SUBDIR})
    string(
      REPLACE "_"
              ""
              LINGLONG_ADD_LIBRARY_SUBDIR
              ${LINGLONG_ADD_LIBRARY_SUBDIR}
    )
    target_link_libraries(
      ${LINGLONG_LIB_NAME}
      ${LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE}
      "${LINGLONG_LIB_PREFIX}${LINGLONG_ADD_LIBRARY_SUBDIR}"
    )
  endforeach()

  if(LINGLONG_ADD_LIBRARY_PARSED_DEPENDENCIES)
    target_link_libraries(
      ${LINGLONG_LIB_NAME}
      ${LINGLONG_ADD_LIBRARY_SUBDIR_LINKTYPE}
      ${LINGLONG_ADD_LIBRARY_PARSED_DEPENDENCIES}
    )
  endif()
endfunction()

add_subdirectory(dbusgen)

add_subdirectory(linglong)
