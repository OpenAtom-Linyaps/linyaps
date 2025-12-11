// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

namespace linglong::utils {

namespace detail {

struct RunInNamespaceArgs
{
    int argc;
    char **argv;
    std::array<int, 2> pair;
};

struct SubuidRange
{
    std::string subuid;
    std::string count;
};

utils::error::Result<std::string> getUserName(uid_t uid);
utils::error::Result<std::vector<SubuidRange>> parseSubuidRanges(std::istream &stream,
                                                                 uid_t uid,
                                                                 const std::string &name);
utils::error::Result<std::vector<SubuidRange>> getSubuidRange(uid_t uid, bool isUid);

} // namespace detail

utils::error::Result<bool> needRunInNamespace();
utils::error::Result<int> runInNamespace(int argc, char **argv);

} // namespace linglong::utils
