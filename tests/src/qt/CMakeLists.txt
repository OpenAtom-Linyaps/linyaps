# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(linglong__qttests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Test Widgets)

add_executable(repo_client_test repo/repo_client_test.cpp util/mock-network.h util/mock-network.cpp)
add_test(repo_client_test repo_client_test)
target_link_libraries(repo_client_test PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Test
    Qt5::Widgets
    linglong
)
install(TARGETS repo_client_test
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)