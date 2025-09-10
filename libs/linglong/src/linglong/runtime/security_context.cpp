// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/runtime/security_context.h"

#include "linglong/utils/log/log.h"

#ifdef WAYLAND_SEC_CTX_SUPPORT
#  include "linglong/runtime/wayland_security_ctx.h"
#endif

namespace linglong::runtime {

std::string fromType(SecurityContextType type) noexcept
{
    switch (type) {
    case SecurityContextType::WAYLAND:
        return "wayland";
    case SecurityContextType::UNKNOWN:
        [[fallthrough]];
    default:
        return "unknown";
    }
}

SecurityContextType toType(const std::string &type) noexcept
{
    if (type == "wayland") {
        return SecurityContextType::WAYLAND;
    }
    return SecurityContextType::UNKNOWN;
}

std::vector<SecurityContextType> &getDefaultSecurityContexts() noexcept
{
    static std::vector<SecurityContextType> ctxs = {
#ifdef WAYLAND_SEC_CTX_SUPPORT
        SecurityContextType::WAYLAND
#endif
    };
    return ctxs;
}

std::unique_ptr<SecurityContextManager> getSecurityContextManager(SecurityContextType type) noexcept
try {
    switch (type) {
#ifdef WAYLAND_SEC_CTX_SUPPORT
    case SecurityContextType::WAYLAND:
        return std::make_unique<WaylandSecurityContextManagerV1>();
#else
        [[fallthrough]];
#endif
    default:
        return nullptr;
    }
} catch (const std::runtime_error &e) {
    LogD("failed to create security context manager: {}", e.what());
    return nullptr;
}

} // namespace linglong::runtime
