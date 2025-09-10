// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/runtime/security_context.h"
#include "linglong/utils/error/error.h"
#include "wayland-security-context-v1.h"

namespace linglong::runtime {

class WaylandSecurityContextManagerV1;

class WaylandSecurityContextV1 final : public SecurityContext
{
public:
    WaylandSecurityContextV1() = delete;
    ~WaylandSecurityContextV1() override;
    WaylandSecurityContextV1(const WaylandSecurityContextV1 &) = delete;
    WaylandSecurityContextV1 &operator=(const WaylandSecurityContextV1 &) = delete;
    WaylandSecurityContextV1(WaylandSecurityContextV1 &&other) = delete;
    WaylandSecurityContextV1 &operator=(WaylandSecurityContextV1 &&other) = delete;

    linglong::utils::error::Result<void>
    apply(generator::ContainerCfgBuilder &builder) noexcept override;

    SecurityContextType type() noexcept override { return SecurityContextType::WAYLAND; }

private:
    friend class WaylandSecurityContextManagerV1;
    explicit WaylandSecurityContextV1(int listenFd, int closeFd, std::filesystem::path socketPath);
    std::filesystem::path socketPath;
    int listenFd{ -1 };
    int closeFd{ -1 };
};

class WaylandSecurityContextManagerV1 final : public SecurityContextManager
{
public:
    explicit WaylandSecurityContextManagerV1();
    ~WaylandSecurityContextManagerV1() override;
    WaylandSecurityContextManagerV1(const WaylandSecurityContextManagerV1 &) = delete;
    WaylandSecurityContextManagerV1 &operator=(const WaylandSecurityContextManagerV1 &) = delete;
    WaylandSecurityContextManagerV1(WaylandSecurityContextManagerV1 &&other) = delete;
    WaylandSecurityContextManagerV1 &operator=(WaylandSecurityContextManagerV1 &&other) = delete;

    linglong::utils::error::Result<std::unique_ptr<SecurityContext>>
    createSecurityContext(generator::ContainerCfgBuilder &builder) noexcept override;

    [[nodiscard]] wl_display *getDisplay() const noexcept { return display; };

    [[nodiscard]] wp_security_context_manager_v1 *getManager() const noexcept { return manager; };

private:
    wl_display *display{ nullptr };
    wp_security_context_manager_v1 *manager{ nullptr };
};

} // namespace linglong::runtime
