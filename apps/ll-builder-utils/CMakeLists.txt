# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

if(NOT BUILD_LINGLONG_BUILDER_UTILS_IN_BOX)
    find_program(BUILDER ll-builder NO_CACHE REQUIRED)

    message(STATUS "ll-builder: ${BUILDER}")

    add_custom_target(ll-builder-utils
        COMMAND ${BUILDER} build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
else()
    message(STATUS "build ll-build-export")
    install(FILES ll-builder-export TYPE BIN PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE WORLD_READ WORLD_EXECUTE)
endif()

if(ENABLE_TESTING)
    find_program(BASH_EXECUTABLE bash REQUIRED)
    add_test(
        NAME ll-builder-export-paths
        COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/ll-builder-export-paths.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/ll-builder-export
    )
endif()
