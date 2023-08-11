# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_subdirectory(src)

install(TARGETS ll-system-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
