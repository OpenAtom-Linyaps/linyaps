/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fuzzy_reference.h"

#include <QRegularExpression>
#include <QStringList>

namespace linglong::package {

utils::error::Result<FuzzyReference> FuzzyReference::parse(const QString &raw) noexcept
{
    LINGLONG_TRACE("parse fuzz reference string " + raw);

    static auto regexp = []() noexcept {
        QRegularExpression regexp(
          R"(^(?:(?<channel>[^:]*):)?(?<id>[^\/]*)(?:\/(?<version>[^\/]*)(?:\/(?<architecture>[^\/]*))?)?$)");
        regexp.optimize();
        return regexp;
    }();

    auto matches = regexp.match(raw);
    if (not(matches.isValid() and matches.hasMatch())) {
        return LINGLONG_ERR("regexp mismatched.");
    }

    std::optional<QString> channel = matches.captured("channel");
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
        version = std::move(tmpVersion).value();
    }

    std::optional<Architecture> architecture;
    auto architectureStr = matches.captured("architecture");
    if ((!architectureStr.isEmpty()) && architectureStr != "unknown") {
        auto tmpArchitecture = Architecture::parse(architectureStr.toStdString());
        if (!tmpArchitecture) {
            return LINGLONG_ERR(tmpArchitecture);
        }
        architecture = std::move(tmpArchitecture).value();
    }

    return create(channel, id, version, architecture);
}

utils::error::Result<FuzzyReference>
FuzzyReference::create(const std::optional<QString> &channel,
                       const QString &id, // NOLINT
                       const std::optional<Version> &version,
                       const std::optional<Architecture> &arch) noexcept
try {
    return FuzzyReference(channel, id, version, arch);
} catch (const std::exception &e) {
    LINGLONG_TRACE("create fuzz reference");
    return LINGLONG_ERR(e);
}

FuzzyReference::FuzzyReference(const std::optional<QString> &channel,
                               const QString &id,
                               const std::optional<Version> &version,
                               const std::optional<Architecture> &architecture)
    : channel(channel)
    , id(id)
    , version(version)
    , arch(architecture)
{
    if (channel && channel->isEmpty()) {
        throw std::runtime_error("empty channel");
    }

    if (id.isEmpty()) {
        throw std::runtime_error("empty id");
    }
}

QString FuzzyReference::toString() const noexcept
{
    return QString("%1:%2/%3/%4")
      .arg(this->channel.value_or("unknown"),
           this->id,
           this->version ? this->version->toString() : "unknown",
           this->arch ? this->arch->toString() : "unknown");
}

QString FuzzyReference::toErrString() const noexcept
{
    QString errString;
    if (channel) {
        errString += this->channel.value() + ":";
    }
    errString += this->id;
    if (version) {
        errString += "/" + version.value().toString();
    }
    if (arch) {
        errString += "/" + arch.value().toString();
    }
    return errString;
}

} // namespace linglong::package
