# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
add_subdirectory(loader)

if (NOT ${ENABLE_UAB_HEADER})
    return()
endif()

add_subdirectory(header)
