/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"

namespace linglong::api::types::v1 {

inline bool operator==(const Repo &cfg1, const Repo &cfg2) noexcept
{
    return cfg1.alias == cfg2.alias && cfg1.name == cfg2.name && cfg1.url == cfg2.url;
}

inline bool operator!=(const Repo &cfg1, const Repo &cfg2) noexcept
{
    return !(cfg1 == cfg2);
}

inline bool operator==(const RepoConfig &cfg1, const RepoConfig &cfg2) noexcept
{
    return cfg1.version == cfg2.version && cfg1.repos == cfg2.repos
      && cfg1.defaultRepo == cfg2.defaultRepo;
}

inline bool operator!=(const RepoConfig &cfg1, const RepoConfig &cfg2) noexcept
{
    return !(cfg1 == cfg2);
}

inline bool operator==(const RepoConfigV2 &cfg1, const RepoConfigV2 &cfg2) noexcept
{
    return cfg1.version == cfg2.version && cfg1.repos == cfg2.repos
      && cfg1.defaultRepo == cfg2.defaultRepo;
}

inline bool operator!=(const RepoConfigV2 &cfg1, const RepoConfigV2 &cfg2) noexcept
{
    return !(cfg1 == cfg2);
}

} // namespace linglong::api::types::v1
