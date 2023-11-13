/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/reference.h"

#include <QStringList>

namespace linglong::package {

utils::error::Result<Reference> Reference::parse(const QString &raw) noexcept
try {
    return Reference(raw);
} catch (const std::exception &e) {
    auto err = Err(-1, e.what()).value();
    return EWrap("invalid reference", err);
}

Reference::Reference(const QString &raw)
    : Reference([&raw]() -> QStringList {
        auto ret = raw.split('/');
        if (ret.size() != 5) {
            throw std::runtime_error("incorrect number of components");
        }
        return ret;
    }())
{
}

Reference::Reference(const QStringList &components)
    : id([&components]() {
        const auto &ret = components[0];
        if (ret.isEmpty()) {
            throw std::runtime_error("empty id");
        }
        return ret;
    }())
    , version(components[1])
    , arch(components[2])
    , module(components[3])
    , channel([&components]() {
        const auto &ret = components[4];
        if (ret.isEmpty()) {
            throw std::runtime_error("empty id");
        }
        return ret;
    }())
{
}

QString Reference::toString() const noexcept
{
    return QStringList{ this->id,
                        this->version.toString(),
                        this->arch.toString(),
                        this->module.toString(),
                        this->channel }
      .join("/");
}

} // namespace linglong::package
