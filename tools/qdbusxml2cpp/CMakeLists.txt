# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set(SOURCE qdbusxml2cpp_${QT_VERSION_MAJOR}.cpp)
add_executable(qdbusxml2cpp ${SOURCE})

target_link_libraries(
  qdbusxml2cpp PRIVATE Qt${QT_VERSION_MAJOR}::DBus Qt${QT_VERSION_MAJOR}::Core
                       Qt${QT_VERSION_MAJOR}::DBusPrivate)
