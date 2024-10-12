/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package_manager/package_task.h"

#include <QMetaEnum>
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

void JSONPrinter::printPackages(const std::vector<api::types::v1::PackageInfoV2> &list)
{
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

void JSONPrinter::printRepoConfig(const api::types::v1::RepoConfig &config)
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

std::string stateToString(service::PackageTask::State state)
{
    switch (state) {
    case service::PackageTask::State::Canceled:
        return "Canceled";
    case service::PackageTask::State::Failed:
        return "Failed";
    case service::PackageTask::State::Processing:
        return "Processing";
    case service::PackageTask::State::Pending:
        return "Pending";
    case service::PackageTask::State::Queued:
        return "Queued";
    case service::PackageTask::State::Succeed:
        return "Succeed";
    default:
        return "Unknown State";
    }
}

std::string subStateToString(service::PackageTask::SubState state)
{
    switch (state) {
    case service::PackageTask::SubState::Done:
        return "Done";
    case service::PackageTask::SubState::InstallApplication:
        return "InstallApplication";
    case service::PackageTask::SubState::InstallBase:
        return "InstallBase";
    case service::PackageTask::SubState::InstallRuntime:
        return "InstallRuntime";
    case service::PackageTask::SubState::PostAction:
        return "PostAction";
    case service::PackageTask::SubState::PreAction:
        return "PreAction";
    case service::PackageTask::SubState::PreRemove:
        return "PreAction";
    case service::PackageTask::SubState::Uninstall:
        return "PreAction";
    default:
        return "Unknown SubState";
    }
}

void JSONPrinter::printTaskState(const double percentage,
                                  const QString &message,
                                  const api::types::v1::State state,
                                  const api::types::v1::SubState subState)
{
    nlohmann::json json;
    json["percentage"] = percentage;
    json["message"] = message.toStdString();
    json["state"] = stateToString(state);
    json["subState"] = subStateToString(subState);

    std::cout << json.dump() << std::endl;
}

} // namespace linglong::cli
