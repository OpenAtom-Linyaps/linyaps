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

/*
 * 在线安装软件包
 *
 * @param packageIDList: 软件包的appid
 * @param paramMap: 安装参数
 *
 * @param packageIDList
 */
RetMessageList PackageManagerFlatpakImpl::Install(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    if (packageIDList.size() == 0) {
        qCritical() << "install packageIDList input err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("install packageIDList input err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString pkgName = packageIDList.at(0);
    QStringList argStrList;
    argStrList << "install"
               << "-y" << pkgName;
    argStrList.join(" ");
    auto ret = Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        info->setcode(RetCode(RetCode::pkg_install_failed));
        info->setmessage("install " + pkgName + " failed");
        info->setstate(false);
    } else {
        info->setcode(RetCode(RetCode::pkg_install_success));
        info->setmessage("install " + pkgName + " success");
        info->setstate(true);
    }
    retMsg.push_back(info);
    qInfo() << "flatpak Install " << pkgName << ", ret:" << ret;
    return retMsg;
}

/*
 * 查询软件包
 *
 * @param packageIDList: 软件包的appid
 * @param paramMap: 查询参数
 *
 * @return AppMetaInfoList 查询结果列表
 */
AppMetaInfoList PackageManagerFlatpakImpl::Query(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    AppMetaInfoList pkglist;
    auto info = QPointer<AppMetaInfo>(new AppMetaInfo);
    if (packageIDList.size() == 0) {
        qCritical() << "packageIDList input err";
        return pkglist;
    }

    QString pkgName = packageIDList.at(0);
    QProcess proc;
    QStringList argStrList;
    if (pkgName == "installed") {
        argStrList << "list";
        info->appId = "flatpaklist";
    } else {
        argStrList << "search" << pkgName;
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
    qInfo() << "flatpak Query " << pkgName << ", exitStatus:" << proc.exitStatus() << ", retCode:" << retCode;
    info->description = ret;
    pkglist.push_back(info);
    return pkglist;
}

/*
 * 卸载软件包
 *
 * @param packageIDList: 软件包的appid
 * @param paramMap: 卸载参数
 *
 * @return RetMessageList 卸载结果信息
 */
RetMessageList PackageManagerFlatpakImpl::Uninstall(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    if (packageIDList.size() == 0) {
        qCritical() << "uninstall packageIDList input err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("uninstall packageIDList input err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString pkgName = packageIDList.at(0);
    QStringList argStrList;
    argStrList << "uninstall"
               << "-y" << pkgName;
    argStrList.join(" ");
    auto ret = Runner("flatpak", argStrList, 1000 * 60 * 30);
    if (!ret) {
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall " + pkgName + " failed");
        info->setstate(false);
    } else {
        info->setcode(RetCode(RetCode::pkg_uninstall_success));
        info->setmessage("uninstall " + pkgName + " success");
        info->setstate(true);
    }
    retMsg.push_back(info);
    qInfo() << "flatpak Uninstall " << pkgName << ", ret:" << ret;
    return retMsg;
}