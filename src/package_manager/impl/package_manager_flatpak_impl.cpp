/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager_flatpak_impl.h"

#include "module/util/runner.h"

namespace linglong {
namespace service {
Reply PackageManagerFlatpakImpl::Install(const InstallParamOption &installParamOption)
{
    Reply reply;
    QString appId = installParamOption.appId.trimmed();
    QStringList argStrList;
    argStrList << "install"
               << "--user"
               << "-y" << appId;
    const auto ret = linglong::runner::Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        reply.code = STATUS_CODE(kPkgInstallFailed);
        reply.message = "install " + appId + " failed";
    } else {
        reply.code = STATUS_CODE(kPkgInstallSuccess);
        reply.message = "install " + appId + " success";
    }
    qInfo() << "flatpak Install " << appId << ", ret:" << ret;
    return reply;
}

/*
 * 查询软件包
 *
 * @param paramOption: 查询参数
 *
 * @return QueryReply dbus方法调用应答
 */
QueryReply PackageManagerFlatpakImpl::Query(const QueryParamOption &paramOption)
{
    QueryReply reply;
    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return reply;
    }

    QProcess proc;
    QStringList argStrList;
    QJsonObject appItem;
    if ("installed" == appId) {
        argStrList << "list";
        appItem["appId"] = "flatpaklist";
    } else {
        argStrList << "search" << appId;
        appItem["appId"] = "flatpakquery";
    }
    proc.start("flatpak", argStrList);
    if (!proc.waitForStarted()) {
        reply.code = STATUS_CODE(kErrorPkgQueryFailed);
        reply.message = "flatpak not installed";
        qCritical() << reply.message;
        return reply;
    }
    proc.waitForFinished(1000 * 60 * 5);
    QString ret = proc.readAllStandardOutput();
    const auto retCode = proc.exitCode();
    qInfo() << "flatpak query " << appId << ", exitStatus:" << proc.exitStatus()
            << ", retCode:" << retCode;
    QJsonArray appList;
    appItem["description"] = ret;
    appList.append(appItem);
    QJsonDocument document = QJsonDocument(appList);
    reply.code = STATUS_CODE(kErrorPkgQuerySuccess);
    reply.message = "flatpak query " + appId + " success";
    reply.result = QString(document.toJson());
    return reply;
}

/*
 * 卸载软件包
 *
 * @param paramOption: 卸载命令参数
 *
 * @return Reply 卸载结果信息
 */
Reply PackageManagerFlatpakImpl::Uninstall(const UninstallParamOption &paramOption)
{
    Reply reply;
    QString appId = paramOption.appId.trimmed();
    QStringList argStrList;
    argStrList << "uninstall"
               << "--user"
               << "-y" << appId;
    const auto ret = linglong::runner::Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        reply.code = STATUS_CODE(kPkgUninstallFailed);
        reply.message = "uninstall " + appId + " failed";
    } else {
        reply.code = STATUS_CODE(kPkgUninstallSuccess);
        reply.message = "uninstall " + appId + " success";
    }
    qInfo() << "flatpak Uninstall " << appId << ", ret:" << ret;
    return reply;
}
} // namespace service
} // namespace linglong