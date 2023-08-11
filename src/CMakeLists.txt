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

add_subdirectory(dbusgen)

add_subdirectory(linglong)
