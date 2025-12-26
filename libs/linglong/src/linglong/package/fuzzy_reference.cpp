/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.:
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/fuzzy_reference.h"

#include "linglong/utils/error/error.h"

#include <fmt/format.h>

#include <QRegularExpression>
#include <QStringList>

namespace linglong::package {

utils::error::Result<FuzzyReference> FuzzyReference::parse(const std::string &raw) noexcept
{
    LINGLONG_TRACE("parse fuzzy reference string " + raw);

    static auto regexp = []() noexcept {
        QRegularExpression regexp(
          R"(^(?:(?<channel>[^:]*):)?(?<id>[^\/]*)(?:\/(?<version>[^\/]*)(?:\/(?<architecture>[^\/]*))?)?$)");
        regexp.optimize();
        return regexp;
    }();

    auto matches = regexp.match(QString::fromStdString(raw));
    if (not(matches.isValid() and matches.hasMatch())) {
        return LINGLONG_ERR("invalid fuzzy reference",
                            utils::error::ErrorCode::InvalidFuzzyReference);
    }

    std::optional<std::string> channel;
    if (auto captured = matches.captured("channel"); !captured.isEmpty() && captured != "unknown") {
        channel = captured.toStdString();
    }

    auto id = matches.captured("id").toStdString(); // NOLINT

    std::optional<std::string> version;
    auto versionStr = matches.captured("version");
    if ((!versionStr.isEmpty()) && versionStr != "unknown") {
        version = versionStr.toStdString();
    }

    std::optional<Architecture> architecture;
    auto architectureStr = matches.captured("architecture");
    if ((!architectureStr.isEmpty()) && architectureStr != "unknown") {
        auto tmpArchitecture = Architecture::parse(architectureStr.toStdString());
        if (!tmpArchitecture) {
            return LINGLONG_ERR(tmpArchitecture.error().message(),
                                utils::error::ErrorCode::UnknownArchitecture);
        }
        architecture = std::move(tmpArchitecture).value();
    }

    return create(channel, id, version, architecture);
}

utils::error::Result<FuzzyReference>
FuzzyReference::create(const std::optional<std::string> &channel,
                       const std::string &id, // NOLINT
                       const std::optional<std::string> &version,
                       const std::optional<Architecture> &arch) noexcept
try {
    return FuzzyReference(channel, id, version, arch);
} catch (const std::exception &e) {
    LINGLONG_TRACE("create fuzzy reference");
    return LINGLONG_ERR(e.what(), utils::error::ErrorCode::Unknown);
}

FuzzyReference::FuzzyReference(const std::optional<std::string> &channel,
                               const std::string &id,
                               const std::optional<std::string> &version,
                               const std::optional<Architecture> &architecture)
    : channel(channel)
    , id(id)
    , version(version)
    , arch(architecture)
{
    if (channel && channel->empty()) {
        throw std::runtime_error("empty channel");
    }

    if (id.empty()) {
        throw std::runtime_error("empty id");
    }
}

std::string FuzzyReference::toString() const noexcept
{
    return fmt::format("{}:{}/{}/{}",
                       this->channel.value_or("unknown"),
                       this->id,
                       this->version ? this->version.value() : "unknown",
                       this->arch ? this->arch->toString() : "unknown");
}

} // namespace linglong::package
