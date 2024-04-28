/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/printer.h"

#include "linglong/package_manager/task.h"
#include "linglong/api/types/v1/Generators.hpp"

#include <QJsonArray>

#include <iomanip>
#include <iostream>

namespace linglong::cli {

void Printer::printErr(const utils::error::Error &err)
{
    std::cout << "Error: CODE=" << err.code() << std::endl
              << err.message().toStdString() << std::endl;
}

void Printer::printPackages(const std::vector<api::types::v1::PackageInfo> &list)
{
    std::cout << "\033[38;5;214m" << std::left << std::setw(32) << qUtf8Printable("appId")
              << std::setw(32) << qUtf8Printable("name") << std::setw(16)
              << qUtf8Printable("version") << std::setw(12) << qUtf8Printable("arch")
              << std::setw(16) << qUtf8Printable("channel") << std::setw(12)
              << qUtf8Printable("module") << qUtf8Printable("description") << "\033[0m"
              << std::endl;

    for (const auto &info : list) {
        this->printPackageInfo(info);
    }
}

void Printer::printContainers(const std::vector<api::types::v1::CliContainer> &list)
{
    std::cout << "\033[38;5;214m" << std::left << std::setw(48) << qUtf8Printable("App")
              << std::setw(36) << qUtf8Printable("ContainerID") << std::setw(8)
              << qUtf8Printable("Pid") << qUtf8Printable("Path") << "\033[0m" << std::endl;

    for (auto const &container : list) {
        std::cout << std::setw(48) << container.package << std::setw(36) << container.id
                  << std::setw(8) << container.pid << std::endl;
    }
}

void Printer::printReply(const api::types::v1::CommonResult &reply)
{
    std::cout << "code: " << reply.code << std::endl;
    std::cout << "message:" << std::endl << reply.message << std::endl;
}

void Printer::printRepoConfig(const api::types::v1::RepoConfig &repoInfo)
{
    std::cout << "Default: " << repoInfo.defaultRepo << std::endl;
    std::cout << std::left << std::setw(11) << "Name";
    std::cout << "Url" << std::endl;
    for (const auto &repo : repoInfo.repos) {
        std::cout << std::left << std::setw(10) << repo.first << " " << repo.second << std::endl;
    }
}

void Printer::printLayerInfo(const api::types::v1::LayerInfo &info)
{
    std::cout << info.info.dump(4) << std::endl;
}

void Printer::printContent(const QStringList &filePaths)
{
    for (const auto &path : filePaths) {
        std::cout << path.toStdString() << std::endl;
    }
}

void Printer::printTaskStatus(const QString &percentage, const QString &message, int /*status*/)
{
    std::cout << "\r\33[K"
              << "\033[?25l" << percentage.toStdString() << "% " << message.toStdString()
              << "\033[?25h";
    std::cout.flush();
}

void Printer::printPackageInfo(const api::types::v1::PackageInfo &info)
{
    auto simpleDescription = QString::fromStdString(info.description.value_or("")).trimmed();
    if (simpleDescription.length() > 56) {
        simpleDescription = simpleDescription.left(53) + "...";
    }

    auto appId = QString::fromStdString(info.appid).trimmed();

    auto name = QString::fromStdString(info.name).trimmed();
    if (name.length() > 32) {
        name = name.left(29) + "...";
    }
    if (appId.length() > 32) {
        name.push_front(" ");
    }
    int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
    std::cout << std::setw(32) << appId.toStdString() << std::setw(32) << name.toStdString()
              << std::setw(16) << info.version << std::setw(12) << info.arch[0] << std::setw(16)
              << info.channel << std::setw(12) << info.packageInfoModule << std::setw(length)
              << simpleDescription.toStdString() << std::endl;
}

void Printer::printPackage(const api::types::v1::PackageInfo &info)
{
    std::cout << nlohmann::json(info).dump(4)<<std::endl;
}
} // namespace linglong::cli
