// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <optional>

namespace linglong::utils {

namespace detail {

struct RunInNamespaceArgs
{
    int argc;
    char **argv;
    std::array<int, 2> pair;
    std::optional<uid_t> uid;
    std::optional<gid_t> gid;
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

utils::error::Result<std::string> getSelfExe();
utils::error::Result<bool> needRunInNamespace();
utils::error::Result<int> runInNamespace(int argc,
                                         char **argv,
                                         std::optional<uid_t> uid = std::nullopt,
                                         std::optional<gid_t> gid = std::nullopt);
void checkPauseDebugger();

} // namespace linglong::utils
