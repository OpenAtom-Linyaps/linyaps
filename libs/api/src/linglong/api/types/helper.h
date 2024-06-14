/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_API_TYPES_HELPER_H_
#define LINGLONG_API_TYPES_HELPER_H_

#include "linglong/api/types/v1/RepoConfig.hpp"

namespace linglong::api::types::v1 {

inline bool operator==(const RepoConfig &cfg1, const RepoConfig &cfg2) noexcept
{
    return cfg1.version == cfg2.version && cfg1.repos == cfg2.repos
      && cfg1.defaultRepo == cfg2.defaultRepo;
}

inline bool operator!=(const RepoConfig &cfg1, const RepoConfig &cfg2) noexcept
{
    return !(cfg1 == cfg2);
}

} // namespace linglong::api::types::v1
#endif
