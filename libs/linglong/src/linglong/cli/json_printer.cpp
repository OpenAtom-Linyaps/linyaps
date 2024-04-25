/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package_manager/task.h"

#include <qmetaobject.h>

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

void JSONPrinter::printPackage(const api::types::v1::PackageInfo &info)
{
    std::cout << nlohmann::json(info).dump() << std::endl;
}

void JSONPrinter::printPackages(const std::vector<api::types::v1::PackageInfo> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
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

void JSONPrinter::printTaskStatus(const QString &percentage, const QString &message, int status)
{
    QJsonArray jsonArray;

    jsonArray.push_back(QJsonObject{
      { "percentage", percentage },
      { "message", message },
      { "state", QMetaEnum::fromType<service::InstallTask::Status>().valueToKey(status) },
    });

    std::cout << QString::fromUtf8(QJsonDocument(jsonArray).toJson()).toStdString() << std::endl;
}

} // namespace linglong::cli
