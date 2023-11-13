/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_FUZZ_REFERENCE_H_
#define LINGLONG_PACKAGE_FUZZ_REFERENCE_H_

#include "linglong/package/architecture.h"
#include "linglong/package/module.h"
#include "linglong/package/version.h"

#include <QString>

#include <optional>

namespace linglong::package {

// This class is a fuzz reference to a tier, use to search a tier.
struct FuzzReference final
{
    static utils::error::Result<FuzzReference> parse(const QString &raw) noexcept;
    explicit FuzzReference(const QString &raw);

    QString id;
    std::optional<Version> version;
    std::optional<Architecture> arch;
    std::optional<Module> module;
    std::optional<QString> channel;

    QString toString() const noexcept;

private:
    explicit FuzzReference(const QStringList &components);
};

} // namespace linglong::package

#endif
