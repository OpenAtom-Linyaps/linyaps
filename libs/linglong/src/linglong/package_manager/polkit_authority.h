// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <functional>
#include <string>
#include <string_view>

namespace linglong::service {

struct PolkitSubjectIdentity
{
    std::string_view kind;
    std::string value;
};

[[nodiscard]] PolkitSubjectIdentity selectPolkitSubject(std::string_view actionId,
                                                         std::string_view busName,
                                                         std::string_view sessionId);

class PolkitAuthority
{
public:
    PolkitAuthority() = delete;

    // Checks whether `systemBusName` may perform `actionId`.
    //
    // polkit remembers the authorizations it grants for `auth_*_keep` actions
    // per subject, and a `system-bus-name` subject is resolved to the calling
    // process. As ll-cli is started from scratch for every operation, that key
    // never repeats and the user would have to authenticate for every command
    // line. A keepable action uses the caller's login session as its polkit
    // subject, letting polkit own the retention and revocation rules.
    static void checkAuthorizationAsync(const std::string &actionId,
                                        const std::string &systemBusName,
                                        std::function<void(utils::error::Result<bool>)> callback,
                                        bool userInteraction = true);
};

} // namespace linglong::service
