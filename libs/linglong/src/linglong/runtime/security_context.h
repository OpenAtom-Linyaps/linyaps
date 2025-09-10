// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/utils/error/error.h"

namespace linglong::runtime {

enum class SecurityContextType {
    WAYLAND,
    UNKNOWN,
};

std::string fromType(SecurityContextType type) noexcept;
SecurityContextType toType(const std::string &type) noexcept;

class SecurityContext
{
    friend class SecurityContextManager;

public:
    virtual ~SecurityContext() = default;
    SecurityContext(const SecurityContext &) = delete;
    SecurityContext &operator=(const SecurityContext &) = delete;
    SecurityContext(SecurityContext &&other) noexcept = default;
    SecurityContext &operator=(SecurityContext &&other) noexcept = default;

    virtual linglong::utils::error::Result<void> apply(generator::ContainerCfgBuilder &builder) = 0;
    virtual SecurityContextType type() noexcept = 0;

protected:
    SecurityContext() = default;
};

class SecurityContextManager
{
public:
    virtual ~SecurityContextManager() = default;
    SecurityContextManager(const SecurityContextManager &) = delete;
    SecurityContextManager &operator=(const SecurityContextManager &) = delete;
    SecurityContextManager(SecurityContextManager &&other) noexcept = default;
    SecurityContextManager &operator=(SecurityContextManager &&other) noexcept = default;

    virtual linglong::utils::error::Result<std::unique_ptr<SecurityContext>>
    createSecurityContext(generator::ContainerCfgBuilder &builder) = 0;

protected:
    SecurityContextManager() = default;
};

std::vector<SecurityContextType> &getDefaultSecurityContexts() noexcept;

std::unique_ptr<SecurityContextManager>
getSecurityContextManager(SecurityContextType type) noexcept;

} // namespace linglong::runtime
