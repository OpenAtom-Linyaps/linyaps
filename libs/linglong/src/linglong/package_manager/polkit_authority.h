// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/package_manager/polkit_authorization_cache.h"
#include "linglong/utils/error/error.h"

#include <functional>
#include <string>

namespace linglong::service {

class PolkitAuthority
{
public:
    PolkitAuthority() = delete;

    // Resolves the session scoped identity of the D-Bus caller behind
    // `systemBusName`. The uid and the pid are read from the bus daemon, which
    // owns them, and the pid is only used to find the login session of the
    // caller, so the result does not change while the user stays logged in.
    // Fails when the caller is already gone or does not belong to a login
    // session, in which case the identity must not be remembered.
    [[nodiscard]] static utils::error::Result<PolkitCallerIdentity>
    resolveCallerIdentity(const std::string &systemBusName) noexcept;

    // Checks whether `systemBusName` may perform `actionId`.
    //
    // polkit remembers the authorizations it grants for `auth_*_keep` actions
    // per subject, and a `system-bus-name` subject is resolved to the calling
    // process. As ll-cli is started from scratch for every operation, that key
    // never repeats and the user would have to authenticate for every command
    // line. The result of a keepable action is therefore remembered for the
    // session of the caller and reused until the window in which polkit itself
    // would have kept the authorization ends.
    static void checkAuthorizationAsync(const std::string &actionId,
                                        const std::string &systemBusName,
                                        std::function<void(utils::error::Result<bool>)> callback,
                                        bool userInteraction = true);
};

} // namespace linglong::service
