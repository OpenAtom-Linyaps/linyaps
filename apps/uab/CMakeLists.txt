# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
if (NOT ${ENABLE_UAB})
    return()
endif()

add_subdirectory(loader)
add_subdirectory(header)
