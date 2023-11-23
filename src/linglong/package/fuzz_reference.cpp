/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fuzz_reference.h"

#include <QStringList>

namespace linglong::package {

utils::error::Result<FuzzReference> FuzzReference::parse(const QString &raw) noexcept
try {
    return FuzzReference(raw);
} catch (const std::exception &e) {
    auto err = LINGLONG_ERR(-1, e.what()).value();
    return LINGLONG_EWRAP("invalid fuzz reference", err);
}

FuzzReference::FuzzReference(const QStringList &components)
    : id([&components]() {
        const auto &ret = components[0];
        if (ret.isEmpty()) {
            throw std::runtime_error("empty id");
        }
        return ret;
    }())
    , version([&components]() -> std::optional<Version> {
        if (components.size() <= 1 || components[1] == "unknown") {
            return std::nullopt;
        }
        return Version::parse(components[1]).value();
    }())
    , arch([&components]() -> std::optional<Architecture> {
        if (components.size() <= 2 || components[2] == "unknown") {
            return std::nullopt;
        }
        return Architecture::parse(components[2]).value();
    }())
    , module([&components]() -> std::optional<Module> {
        if (components.size() <= 3 || components[3] == "unknown") {
            return std::nullopt;
        }
        return Module::parse(components[3]).value();
    }())
    , channel([&components]() -> std::optional<QString> {
        if (components.size() <= 4 || components[4] == "unknown") {
            return std::nullopt;
        }
        return components[4];
    }())
{
}

FuzzReference::FuzzReference(const QString &raw)
    : FuzzReference(raw.split('/'))
{
}

QString FuzzReference::toString() const noexcept
{
    return QStringList{
        this->id,
        [this]() {
            return this->version.has_value() ? this->version->toString() : "unknown";
        }(),
        [this]() {
            return this->arch.has_value() ? this->arch->toString() : "unknown";
        }(),
        [this]() {
            return this->module.has_value() ? this->module->toString() : "unknown";
        }(),
        [this]() {
            return this->channel.value_or("unknown");
        }(),
    }
      .join("/");
}

} // namespace linglong::package
