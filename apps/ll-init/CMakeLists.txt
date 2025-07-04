# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

pfl_add_executable(SOURCES ./src/ll-init.cpp OUTPUT_NAME ll-init LIBEXEC
                   linglong)

set(LL_INIT_TARGET)

get_real_target_name(LL_INIT_TARGET linglong::ll-init)
target_link_libraries(${LL_INIT_TARGET} PRIVATE stdc++fs)
target_link_options(${LL_INIT_TARGET} PRIVATE -static -static-libgcc
                    -static-libstdc++)
