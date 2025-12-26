/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/reference.h"

#include <fmt/format.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>

namespace linglong::package {

utils::error::Result<Reference> Reference::parse(const std::string &raw) noexcept
{
    LINGLONG_TRACE("parse reference string");

    static auto referenceRegExp = []() noexcept {
        QRegularExpression referenceRegExp(
          R"((?<channel>[^:]+):(?<id>[^\/]+)\/(?<version>[^\/]+)\/(?<architecture>[^\/]+))");
        referenceRegExp.optimize();
        return referenceRegExp;
    }();

    auto matches = referenceRegExp.match(QString::fromStdString(raw));
    if (not(matches.isValid() and matches.hasMatch())) {
        return LINGLONG_ERR("regexp mismatched.", -1);
    }

    auto channel = matches.captured("channel");
    auto id = matches.captured("id");
    auto versionStr = matches.captured("version").toStdString();
    auto architectureStr = matches.captured("architecture");

    auto version = Version::parse(versionStr);
    if (!version) {
        return LINGLONG_ERR(version);
    }

    auto arch = Architecture::parse(architectureStr.toStdString());
    if (!arch) {
        return LINGLONG_ERR(arch);
    }

    auto reference = create(channel.toStdString(), id.toStdString(), *version, *arch);
    if (!reference) {
        return LINGLONG_ERR(reference);
    }

    return *reference;
}

utils::error::Result<Reference>
Reference::fromPackageInfo(const api::types::v1::PackageInfoV2 &info) noexcept
{
    LINGLONG_TRACE("parse reference from package info");

    auto version = package::Version::parse(info.version);
    if (!version) {
        return LINGLONG_ERR(version);
    }
    if (version->isVersionV1() && !version->hasTweak()) {
        return LINGLONG_ERR("version .tweak is required");
    }

    auto architecture = package::Architecture::parse(info.arch[0]);
    if (!architecture) {
        return LINGLONG_ERR(architecture);
    }

    auto reference = package::Reference::create(info.channel, info.id, *version, *architecture);
    if (!reference) {
        return LINGLONG_ERR(reference);
    }

    return reference;
}

utils::error::Result<Reference> Reference::create(const std::string &channel,
                                                  const std::string &id,
                                                  const Version &version,
                                                  const Architecture &arch) noexcept
try {
    return Reference(channel, id, version, arch);
} catch (const std::exception &e) {
    LINGLONG_TRACE("create reference");
    return LINGLONG_ERR("invalid reference", e);
}

Reference::Reference(const std::string &channel,
                     const std::string &id,
                     const Version &version,
                     const Architecture &arch)
    : channel(channel)
    , id(id)
    , version(version)
    , arch(arch)
{
    if (channel.empty()) {
        throw std::runtime_error("empty channel");
    }
    if (id.empty()) {
        throw std::runtime_error("empty id");
    }
}

std::string Reference::toString() const noexcept
{
    return fmt::format("{}:{}/{}/{}", channel, id, version.toString(), arch.toString());
}

bool operator!=(const Reference &lhs, const Reference &rhs) noexcept
{
    return lhs.channel != rhs.channel || lhs.id != rhs.id || lhs.version != rhs.version
      || lhs.arch != rhs.arch;
}

bool operator==(const Reference &lhs, const Reference &rhs) noexcept
{
    return !(lhs != rhs);
}

utils::error::Result<Reference>
Reference::fromBuilderProject(const api::types::v1::BuilderProject &project) noexcept
{
    LINGLONG_TRACE("parse reference from BuilderProject");

    auto version = package::Version::parse(project.package.version);
    if (!version) {
        return LINGLONG_ERR(version);
    }

    auto architecture = package::Architecture::currentCPUArchitecture();
    if (project.package.architecture) {
        auto targetArch = package::Architecture::parse(*project.package.architecture);
        if (!targetArch) {
            return LINGLONG_ERR(targetArch);
        }
        architecture = *targetArch;
    }
    std::string channel = "main";
    if (project.package.channel.has_value()) {
        channel = *project.package.channel;
    }
    auto ref = package::Reference::create(channel, project.package.id, *version, architecture);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    return ref;
}

} // namespace linglong::package
