# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_executable(qdbusxml2cpp qdbusxml2cpp.cpp)

target_link_libraries(qdbusxml2cpp PRIVATE Qt::DBus Qt::Core Qt::DBusPrivate)
