/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "linglong/package/architecture.h"
#include "linglong/package/version.h"

#include <QString>

#include <optional>

namespace linglong::package {

// This class is a fuzz reference to a tier, use to search a tier.
class FuzzyReference final
{
public:
    static utils::error::Result<FuzzyReference> parse(const QString &raw) noexcept;
    static utils::error::Result<FuzzyReference>
    create(const std::optional<QString> &channel,
           const QString &id,
           const std::optional<Version> &version,
           const std::optional<Architecture> &arch) noexcept;

    std::optional<QString> channel;
    QString id;
    std::optional<Version> version;
    std::optional<Architecture> arch;

    QString toString() const noexcept;

private:
    explicit FuzzyReference(const std::optional<QString> &channel,
                            const QString &id,
                            const std::optional<Version> &version,
                            const std::optional<Architecture> &arch);
};

} // namespace linglong::package
