/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fuzz_reference.h"

#include <QStringList>

namespace linglong::package {

utils::error::Result<FuzzReference> FuzzReference::parse(const QString &raw) noexcept
{
    LINGLONG_TRACE("parse fuzz reference string");

    static QRegularExpression regexp(
      R"(^(?:(?<channel>[^:]*):)?(?<id>[^\/]*)(?:\/(?<version>[^\/]*)(?:\/(?<architecture>[^\/]*))?)?$)");

    auto matches = regexp.match(raw);
    if (not(matches.isValid() and matches.hasMatch())) {
        return LINGLONG_ERR("regexp mismatched.");
    }

    std::optional<QString> channel;
    channel = matches.captured("channel");
    if (channel->isEmpty() || channel == "unknown") {
        channel = std::nullopt;
    }

    auto id = matches.captured("id"); // NOLINT

    std::optional<Version> version;
    auto versionStr = matches.captured("version");
    if ((!versionStr.isEmpty()) && versionStr != "unknown") {
        auto tmpVersion = Version::parse(versionStr);
        if (!tmpVersion) {
            return LINGLONG_ERR(tmpVersion);
        }
        version = *tmpVersion;
    }

    std::optional<Architecture> architecture;
    auto architectureStr = matches.captured("architecture");
    if ((!architectureStr.isEmpty()) && architectureStr != "unknown") {
        auto tmpArchitecture = Architecture::parse(architectureStr);
        if (!tmpArchitecture) {
            return LINGLONG_ERR(tmpArchitecture);
        }
        architecture = *tmpArchitecture;
    }

    return create(channel, id, version, architecture);
}

utils::error::Result<FuzzReference>
FuzzReference::create(const std::optional<QString> &channel,
                      const QString &id, // NOLINT
                      const std::optional<Version> &version,
                      const std::optional<Architecture> &arch) noexcept
try {
    return FuzzReference(channel, id, version, arch);
} catch (const std::exception &e) {
    LINGLONG_TRACE("create fuzz reference");
    return LINGLONG_ERR(e);
}

FuzzReference::FuzzReference(const std::optional<QString> &channel,
                             const QString &id,
                             const std::optional<Version> &version,
                             const std::optional<Architecture> &architecture)
    : channel(channel)
    , id(id)
    , version(version)
    , arch(architecture)
{
    if (channel.has_value() && channel->isEmpty()) {
        throw std::runtime_error("empty channel");
    }

    if (id.isEmpty()) {
        throw std::runtime_error("empty id");
    }
}

QString FuzzReference::toString() const noexcept
{
    return QString("%1:%2/%3/%4")
      .arg(this->channel.value_or("unknown"),
           this->id,
           this->version.has_value() ? this->version->toString() : "unknown",
           this->arch.has_value() ? this->arch->toString() : "unknown");
}

} // namespace linglong::package
