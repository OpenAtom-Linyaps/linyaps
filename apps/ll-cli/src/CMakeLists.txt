# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_executable(ll-cli main.cpp)
target_link_libraries(ll-cli PRIVATE linglong)
