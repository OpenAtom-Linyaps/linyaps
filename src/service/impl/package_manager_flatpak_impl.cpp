/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "package_manager_flatpak_impl.h"

linglong::service::Reply
PackageManagerFlatpakImpl::Install(const linglong::service::InstallParamOption &installParamOption)
{
    linglong::service::Reply reply;
    QString appId = installParamOption.appId.trimmed();
    QStringList argStrList;
    argStrList << "install"
               << "--user"
               << "-y" << appId;
    auto ret = Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        reply.code = RetCode(RetCode::pkg_install_failed);
        reply.message = "install " + appId + " failed";
    } else {
        reply.code = RetCode(RetCode::pkg_install_success);
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
 * @return linglong::package::AppMetaInfoList 查询结果列表
 */
linglong::package::AppMetaInfoList
PackageManagerFlatpakImpl::Query(const linglong::service::QueryParamOption &paramOption)
{
    linglong::package::AppMetaInfoList pkglist;
    auto info = QPointer<linglong::package::AppMetaInfo>(new linglong::package::AppMetaInfo);

    QString appId = paramOption.appId.trimmed();
    if (appId.isNull() || appId.isEmpty()) {
        qCritical() << "appId input err";
        return {};
    }

    QProcess proc;
    QStringList argStrList;
    if ("installed" == appId) {
        argStrList << "list";
        info->appId = "flatpaklist";
    } else {
        argStrList << "search" << appId;
        info->appId = "flatpakquery";
    }
    proc.start("flatpak", argStrList);
    if (!proc.waitForStarted()) {
        qCritical() << "flatpak not installed";
        return pkglist;
    }
    proc.waitForFinished(1000 * 60 * 5);
    QString ret = proc.readAllStandardOutput();
    auto retCode = proc.exitCode();
    qInfo() << "flatpak Query " << appId << ", exitStatus:" << proc.exitStatus() << ", retCode:" << retCode;
    info->description = ret;
    pkglist.push_back(info);
    return pkglist;
}

/*
 * 卸载软件包
 *
 * @param paramOption: 卸载命令参数
 *
 * @return linglong::service::Reply 卸载结果信息
 */
linglong::service::Reply
PackageManagerFlatpakImpl::Uninstall(const linglong::service::UninstallParamOption &paramOption)
{
    linglong::service::Reply reply;
    QString appId = paramOption.appId.trimmed();
    QStringList argStrList;
    argStrList << "uninstall"
               << "--user"
               << "-y" << appId;
    auto ret = Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        reply.code = RetCode(RetCode::pkg_uninstall_failed);
        reply.message = "uninstall " + appId + " failed";
    } else {
        reply.code = RetCode(RetCode::pkg_uninstall_success);
        reply.message = "uninstall " + appId + " success";
    }
    qInfo() << "flatpak Uninstall " << appId << ", ret:" << ret;
    return reply;
}