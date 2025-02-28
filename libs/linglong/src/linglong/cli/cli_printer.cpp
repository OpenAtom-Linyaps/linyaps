/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli_printer.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/package/reference.h"
#include "linglong/utils/gettext.h"

#include <QJsonArray>

#include <iomanip>
#include <iostream>

namespace linglong::cli {

std::wstring subwstr(std::wstring wstr, int width)
{
    if (wcswidth(wstr.c_str(), -1) <= width)
        return wstr;
    int halfsize = wstr.size() / 2;
    std::wstring halfwstr = wstr.substr(0, halfsize);
    int halfwidth = wcswidth(halfwstr.c_str(), -1);
    if (halfwidth >= width)
        return subwstr(halfwstr, width);
    return halfwstr + subwstr(wstr.substr(halfsize, halfsize), width - halfwidth);
}

void CLIPrinter::printErr(const utils::error::Error &err)
{
    std::cout << "Error: CODE=" << err.code() << std::endl
              << err.message().toStdString() << std::endl;
}

void CLIPrinter::printPruneResult(const std::vector<api::types::v1::PackageInfoV2> &list)
{
    if (list.size() == 0) {
        std::cout << "No unused base or runtime." << std::endl;
        return;
    }
    std::cout << "Unused base or runtime:" << std::endl;
    for (const auto &info : list) {
        auto ref = package::Reference::fromPackageInfo(info);
        std::cout << ref->toString().toStdString() << std::endl;
    }
    std::cout << list.size() << " unused base or runtime have been removed." << std::endl;
}

// 调整字符串的显示宽度
std::string adjustDisplayWidth(const QString &str, int targetWidth)
{
    int currentWidth{ 0 };
    for (QChar ch : str) {
        int charWidth = wcwidth(ch.unicode());
        if (charWidth > 0) {
            currentWidth += charWidth;
        }
    }
    if (currentWidth >= targetWidth) {
        return str.toStdString();
    }
    return (str + QString(targetWidth - currentWidth, QChar(' '))).toStdString();
}

void CLIPrinter::printPackages(const std::vector<api::types::v1::PackageInfoV2> &list)
{
    std::cout << "\033[38;5;214m" << std::left << adjustDisplayWidth(qUtf8Printable(_("ID")), 43)
              << adjustDisplayWidth(qUtf8Printable(_("Name")), 33)
              << adjustDisplayWidth(qUtf8Printable(_("Version")), 16)
              << adjustDisplayWidth(qUtf8Printable(_("Channel")), 16)
              << adjustDisplayWidth(qUtf8Printable(_("Module")), 12)
              << qUtf8Printable(_("Description")) << "\033[0m" << std::endl;
    for (const auto &info : list) {
        auto simpleDescription = QString::fromStdString(info.description.value_or("")).simplified();
        auto simpleDescriptionWStr = simpleDescription.toStdWString();
        auto simpleDescriptionWcswidth = wcswidth(simpleDescriptionWStr.c_str(), -1);
        if (simpleDescriptionWcswidth > 56) {
            simpleDescriptionWStr = subwstr(simpleDescriptionWStr, 53) + L"...";
            simpleDescription = QString::fromStdWString(simpleDescriptionWStr);
        }

        auto name = QString::fromStdString(info.name).simplified();
        auto nameWStr = name.toStdWString();
        auto nameWcswidth = wcswidth(nameWStr.c_str(), -1);
        if (nameWcswidth > 33) {
            nameWStr = subwstr(nameWStr, 29) + L"...";
            nameWcswidth = wcswidth(nameWStr.c_str(), -1);
            name = QString::fromStdWString(nameWStr);
        }
        auto nameStr = name.toStdString();
        auto nameOffset = nameStr.size() - nameWcswidth;
        std::cout << std::setw(43) << info.id + " " << std::setw(33 + nameOffset) << nameStr + " "
                  << std::setw(16) << info.version + " " << std::setw(16) << info.channel + " "
                  << std::setw(12) << info.packageInfoV2Module + " "
                  << simpleDescription.toStdString() << std::endl;
    }
}

void CLIPrinter::printContainers(const std::vector<api::types::v1::CliContainer> &list)
{
    if (list.empty()) {
        std::cout << _("No containers are running.") << std::endl;
        return;
    }
    const std::size_t padding{ 2 };
    const std::string packageSection = qUtf8Printable(_("App"));
    const std::string idSection = qUtf8Printable(_("ContainerID"));
    const std::string pidSection = qUtf8Printable(_("Pid"));

    std::size_t packageLen = 0;
    std::size_t idLen = 0;
    std::size_t pidLen = 0;

    std::vector<std::string> packageNames;
    packageNames.reserve(list.size());

    std::for_each(
      list.cbegin(),
      list.cend(),
      [&packageLen, &idLen, &pidLen, &packageNames](const api::types::v1::CliContainer &con) {
          std::string package = con.package;
          size_t colonPos = package.find(':');
          size_t slashPos = package.find('/');
          std::string packageName = package;
          if (colonPos != std::string::npos && slashPos != std::string::npos) {
              packageName = package.substr(colonPos + 1, slashPos - colonPos - 1);
          }

          packageNames.push_back(packageName);
          packageLen = std::max(packageLen, packageName.size());
          idLen = std::max(idLen, con.id.size());
          pidLen = std::max(pidLen, std::to_string(con.pid).size());
      });

    packageLen = std::max(packageSection.size(), packageLen);
    idLen = std::max(idSection.size(), idLen);
    pidLen = std::max(pidSection.size(), pidLen);

    packageLen += padding;
    idLen += padding;
    pidLen += padding;

    std::cout << "\033[38;5;214m" << std::left << std::setw(static_cast<int>(packageLen))
              << adjustDisplayWidth(QString::fromStdString(packageSection), packageLen)
              << std::setw(static_cast<int>(idLen))
              << adjustDisplayWidth(QString::fromStdString(idSection), idLen)
              << std::setw(static_cast<int>(pidLen))
              << adjustDisplayWidth(QString::fromStdString(pidSection), pidLen) << "\033[0m"
              << std::endl;
    for (std::size_t i = 0; i < list.size(); ++i) {
        std::cout << std::setw(static_cast<int>(packageLen)) << packageNames[i]
                  << std::setw(static_cast<int>(idLen)) << list[i].id
                  << std::setw(static_cast<int>(pidLen)) << list[i].pid << std::endl;
    }
}

void CLIPrinter::printReply(const api::types::v1::CommonResult &reply)
{
    std::cout << "code: " << reply.code << std::endl;
    std::cout << "message:" << std::endl << reply.message << std::endl;
}

void CLIPrinter::printRepoConfig(const api::types::v1::RepoConfigV2 &repoInfo)
{
    std::cout << "Default: " << repoInfo.defaultRepo << std::endl;
    // get the max length of url
    size_t maxUrlLength = 0;
    for (const auto &repo : repoInfo.repos) {
        maxUrlLength = std::max(maxUrlLength, repo.url.size());
    }
    std::cout << std::left << std::setw(11) << _("Name");
    std::cout << std::setw(maxUrlLength + 2) << _("Url") << std::setw(11) << _("Alias")
              << std::setw(10) << _("Priority") << std::endl;

    auto repos = repoInfo.repos;
    // 按照优先级从高到低排序
    std::sort(repos.begin(),
              repos.end(),
              [](const api::types::v1::Repo &a, const api::types::v1::Repo &b) {
                  return a.priority > b.priority;
              });
    for (const auto &repo : repos) {
        std::cout << std::left << std::setw(11) << repo.name << std::setw(maxUrlLength + 2)
                  << repo.url << std::setw(11) << repo.alias.value_or(repo.name) << std::setw(10)
                  << repo.priority << std::endl;
    }
}

void CLIPrinter::printLayerInfo(const api::types::v1::LayerInfo &info)
{
    std::cout << info.info.dump(4) << std::endl;
}

void CLIPrinter::printContent(const QStringList &filePaths)
{
    for (const auto &path : filePaths) {
        std::cout << path.toStdString() << std::endl;
    }
}

void CLIPrinter::printTaskState(double percentage,
                                const QString &message,
                                api::types::v1::State state,
                                api::types::v1::SubState subState)
{
    auto &stdout = std::cout;
    if (state == api::types::v1::State::Failed) {
        stdout << "\r\33[K"
               << "\033[?25l" << message.toStdString() << "\033[?25h";
    } else {
        stdout << "\r\33[K"
               << "\033[?25l" << message.toStdString() << ":" << percentage << "%"
               << "\033[?25h";
    }
    if (state == api::types::v1::State::PartCompleted
        || subState == api::types::v1::SubState::AllDone
        || subState == api::types::v1::SubState::PackageManagerDone) {
        stdout << "\n";
    }
    stdout.flush();
}

void CLIPrinter::printPackage(const api::types::v1::PackageInfoV2 &info)
{
    std::cout << nlohmann::json(info).dump(4) << std::endl;
}

void CLIPrinter::printUpgradeList(std::vector<api::types::v1::UpgradeListResult> &list)
{
    if (list.empty()) {
        std::cout << _("No apps available for update.") << std::endl;
        return;
    }

    std::size_t idLen{ 0 }, installedLen{ 0 };

    std::for_each(list.cbegin(), list.cend(), [&idLen, &installedLen](const auto &info) {
        idLen = std::max(idLen, info.id.size()) + 2;
        installedLen = std::max(installedLen, info.oldVersion.size()) + 2;
    });

    std::cout << "\033[38;5;214m" << std::left << std::setw(idLen)
              << adjustDisplayWidth(qUtf8Printable(_("ID")), idLen) << std::setw(installedLen)
              << adjustDisplayWidth(qUtf8Printable(_("Installed")), installedLen)
              << qUtf8Printable(_("New")) << "\033[0m" << std::endl;

    for (const auto &result : list) {
        std::cout << std::setw(idLen) << result.id << std::setw(installedLen) << result.oldVersion
                  << result.newVersion << std::endl;
    }
}

void CLIPrinter::printInspect(const api::types::v1::InspectResult &result)
{
    std::cout << "appID:\t" << (result.appID.has_value() ? result.appID.value() : "none")
              << std::endl;
}

} // namespace linglong::cli
