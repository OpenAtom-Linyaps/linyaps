/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang@uniontech.com
 *
 * Maintainer: yuanqiliang@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDBusArgument>

namespace linglong {
namespace service {

/**
 * @brief DBus方法调用参数
 * @details service服务公共请求参数
 */
class ParamOption
{
public:
    QString appId; ///< 应用appId
    QString version; ///< 应用版本
    QString arch; ///< 应用架构
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const ParamOption &paramOption)
{
    argument.beginStructure();
    argument << paramOption.appId << paramOption.version << paramOption.arch;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, ParamOption &paramOption)
{
    argument.beginStructure();
    argument >> paramOption.appId >> paramOption.version >> paramOption.arch;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Download方法调用参数
 */
class DownloadParamOption : public ParamOption
{
public:
    QString savePath; ///< 下载文件保存路径
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const DownloadParamOption &downloadParamOption)
{
    argument.beginStructure();
    argument << downloadParamOption.appId << downloadParamOption.version << downloadParamOption.arch
             << downloadParamOption.savePath;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, DownloadParamOption &downloadParamOption)
{
    argument.beginStructure();
    argument >> downloadParamOption.appId >> downloadParamOption.version >> downloadParamOption.arch
        >> downloadParamOption.savePath;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Install方法调用参数
 */
class InstallParamOption : public ParamOption
{
public:
    QString repoPoint; ///< 是否安装非玲珑格式应用
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const InstallParamOption &installParamOption)
{
    argument.beginStructure();
    argument << installParamOption.appId << installParamOption.version << installParamOption.arch
             << installParamOption.repoPoint;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, InstallParamOption &installParamOption)
{
    argument.beginStructure();
    argument >> installParamOption.appId >> installParamOption.version >> installParamOption.arch
        >> installParamOption.repoPoint;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Query方法调用参数
 */
class QueryParamOption : public ParamOption
{
public:
    bool force = false; ///< 是否强制从服务端查询
    QString repoPoint; ///< 是否安装非玲珑格式应用
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const QueryParamOption &paramOption)
{
    argument.beginStructure();
    argument << paramOption.appId << paramOption.version << paramOption.arch << paramOption.force
             << paramOption.repoPoint;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, QueryParamOption &paramOption)
{
    argument.beginStructure();
    argument >> paramOption.appId >> paramOption.version >> paramOption.arch >> paramOption.force
        >> paramOption.repoPoint;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Uninstall方法调用参数
 */
class UninstallParamOption : public ParamOption
{
public:
    QString repoPoint; ///< 是否安装非玲珑格式应用
    bool delAllVersion = false; ///< 是否卸载所有版本应用
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const UninstallParamOption &paramOption)
{
    argument.beginStructure();
    argument << paramOption.appId << paramOption.version << paramOption.arch << paramOption.repoPoint
             << paramOption.delAllVersion;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, UninstallParamOption &paramOption)
{
    argument.beginStructure();
    argument >> paramOption.appId >> paramOption.version >> paramOption.arch >> paramOption.repoPoint
        >> paramOption.delAllVersion;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Run方法调用参数
 */
class RunParamOption : public ParamOption
{
public:
    QString exec; ///< 运行命令,如：/bin/bash
    QString repoPoint; ///< 非玲珑格式应用
    bool noDbusProxy = false; ///< 是否不使用dbus代理
    QString busType = "session"; ///< dbus总线类型，默认session总线
    QString filterName; ///< DBus过滤名称
    QString filterPath; ///< DBus过滤路径
    QString filterInterface; ///< DBus过滤接口
    QString appEnv; ///< 应用环境变量
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const RunParamOption &paramOption)
{
    argument.beginStructure();
    argument << paramOption.appId << paramOption.version << paramOption.arch << paramOption.exec
             << paramOption.repoPoint << paramOption.noDbusProxy << paramOption.busType << paramOption.filterName
             << paramOption.filterPath << paramOption.filterInterface << paramOption.appEnv;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RunParamOption &paramOption)
{
    argument.beginStructure();
    argument >> paramOption.appId >> paramOption.version >> paramOption.arch >> paramOption.exec
        >> paramOption.repoPoint >> paramOption.noDbusProxy >> paramOption.busType >> paramOption.filterName
        >> paramOption.filterPath >> paramOption.filterInterface >> paramOption.appEnv;
    argument.endStructure();
    return argument;
}

/**
 * @brief DBus Exec 方法调用参数
 */
class ExecParamOption
{
public:
    QString containerID; ///< 需要执行命令的容器的ID
    QString cmd; ///< 需要执行的命令
    QString env; ///< 该命令需要的额外环境变量
    QString cwd; ///< 该命令执行时的工作目录（容器中）
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const ExecParamOption &paramOption)
{
    argument.beginStructure();
    argument << paramOption.containerID << paramOption.cmd << paramOption.env << paramOption.cwd;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, ExecParamOption &paramOption)
{
    argument.beginStructure();
    argument >> paramOption.containerID >> paramOption.cmd >> paramOption.env >> paramOption.cwd;
    argument.endStructure();
    return argument;
}

} // namespace service
} // namespace linglong

Q_DECLARE_METATYPE(linglong::service::ParamOption)
Q_DECLARE_METATYPE(linglong::service::DownloadParamOption)
Q_DECLARE_METATYPE(linglong::service::InstallParamOption)
Q_DECLARE_METATYPE(linglong::service::QueryParamOption)
Q_DECLARE_METATYPE(linglong::service::UninstallParamOption)
Q_DECLARE_METATYPE(linglong::service::RunParamOption)
Q_DECLARE_METATYPE(linglong::service::ExecParamOption)
