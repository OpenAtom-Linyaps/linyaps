/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/cli.h"
#include "linglong/package/version.h"

#include <QJsonArray>

#include <iostream>

namespace linglong::cli {

void JSONPrinter::printErr(const utils::error::Error &err)
{
    std::cout << nlohmann::json{
        { "code", err.code() },
        { "message", err.message().toStdString() }
    }.dump() << std::endl;
}

void JSONPrinter::printPackage(const api::types::v1::PackageInfoV2 &info)
{
    std::cout << nlohmann::json(info).dump() << std::endl;
}

void JSONPrinter::printPackages(const std::vector<api::types::v1::PackageInfoDisplay> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printPackages(const std::vector<api::types::v1::PackageInfoV2> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printSearchResult(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> list)
{
    // 搜索结果排序, 优先级为 repo > id > channel > module > version, 高版本在前
    for (auto &[repo, packages] : list) {
        std::sort(packages.begin(), packages.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.id != rhs.id)
                return lhs.id < rhs.id;
            if (lhs.channel != rhs.channel)
                return lhs.channel < rhs.channel;
            if (lhs.packageInfoV2Module != rhs.packageInfoV2Module)
                return lhs.packageInfoV2Module < rhs.packageInfoV2Module;

            auto lhsVer = package::Version::parse(lhs.version.c_str());
            if (!lhsVer) {
                return false;
            }
            auto rhsVer = package::Version::parse(rhs.version.c_str());
            if (!rhsVer) {
                return false;
            }

            return *lhsVer > *rhsVer;
        });
    }

    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &list)
{
    printPackages(list);
}

void JSONPrinter::printContainers(const std::vector<api::types::v1::CliContainer> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printReply(const api::types::v1::CommonResult &reply)
{
    std::cout << nlohmann::json(reply).dump() << std::endl;
}

void JSONPrinter::printRepoConfig(const api::types::v1::RepoConfigV2 &config)
{
    std::cout << nlohmann::json(config).dump() << std::endl;
}

void JSONPrinter::printLayerInfo(const api::types::v1::LayerInfo &info)
{
    std::cout << nlohmann::json(info).dump() << std::endl;
}

void JSONPrinter::printContent(const QStringList &filePaths)
{
    QJsonObject obj{ { "content", QJsonArray::fromStringList(filePaths) } };

    std::cout << QString::fromUtf8(QJsonDocument(obj).toJson()).toStdString() << std::endl;
}

void JSONPrinter::printTaskState(const double percentage,
                                 const QString &message,
                                 api::types::v1::State state,
                                 api::types::v1::SubState subState)
{
    nlohmann::json json;
    json["percentage"] = percentage;
    json["message"] = message.toStdString();
    json["state"] = linglong::cli::toString(state);
    json["subState"] = linglong::cli::toString(subState);

    std::cout << json.dump() << std::endl;
}

void JSONPrinter::printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &list)
{
    std::cout << nlohmann::json(list).dump(4) << std::endl;
}

void JSONPrinter::printInspect(const api::types::v1::InspectResult &result)
{
    std::cout << nlohmann::json(result).dump(4) << std::endl;
}

void JSONPrinter::printMessage(const QString &message)
{
    std::cout << nlohmann::json{ { "message", message.toStdString() } }.dump() << std::endl;
}

} // namespace linglong::cli
