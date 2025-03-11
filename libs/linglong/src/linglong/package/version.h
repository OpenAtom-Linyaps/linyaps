/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/package/fallback_version.h"
#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"
#include "linglong/utils/error/error.h"

#include <QString>

#include <variant>

namespace linglong::package {

struct ParseOptions
{
    bool strict = true;   // 是否严格解析
    bool fallback = true; // 是否允许回退到 V1 解析
};
class Version final
{
public:
    static utils::error::Result<Version> parse(const QString &raw,
                                               const ParseOptions parseOpt = {
                                                 .strict = true, .fallback = true }) noexcept;
    static utils::error::Result<void> validateDependVersion(const QString &raw) noexcept;
    explicit Version(const QString &raw) = delete;

    explicit Version(const VersionV1 &version) { this->version = version; };

    explicit Version(const VersionV2 &version) { this->version = version; };

    explicit Version(const FallbackVersion &version) { this->version = version; };

    void ignoreTweak() noexcept;
    bool isVersionV1() noexcept;
    bool hasTweak() noexcept;

    bool operator==(const Version &that) const noexcept;
    bool operator!=(const Version &that) const noexcept;
    bool operator<(const Version &that) const noexcept;
    bool operator>(const Version &that) const noexcept;
    bool operator<=(const Version &that) const noexcept;
    bool operator>=(const Version &that) const noexcept;

    QString toString() const noexcept;

private:
    std::variant<VersionV2, VersionV1, FallbackVersion> version;
};

} // namespace linglong::package
