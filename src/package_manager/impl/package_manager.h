/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "module/dbus_ipc/package_manager_param.h"
#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/register_meta_type.h"
#include "module/dbus_ipc/reply.h"
#include "module/package/package.h"
#include "module/runtime/container.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QFuture>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

namespace linglong {
namespace service {
class PackageManagerPrivate;

/**
 * @brief The PackageManager class
 * @details PackageManager is a singleton class, and it is used to manage the package
 *          information.
 */
class PackageManager : public QObject,
                       protected QDBusContext,
                       public linglong::util::Singleton<PackageManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManager")

    friend class linglong::util::Singleton<PackageManager>;

public Q_SLOTS:

    /**
     * @brief 修改仓库url
     *
     * @param name 仓库name
     * @param url 仓库url地址
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply ModifyRepo(const QString &name, const QString &url);

    /**
     * @brief 查询软件包下载安装状态
     *
     * @param paramOption 查询参数
     * @param type 查询类型 0:查询应用安装进度 1:查询应用更新进度
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply GetDownloadStatus(const ParamOption &paramOption, int type);

    /**
     * @brief 安装软件包
     *
     * @param installParamOption 安装参数
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply Install(const InstallParamOption &installParamOption);

    /**
     * @brief 卸载软件包
     *
     * @param paramOption 卸载参数
     *
     * @return Reply 同Install
     */
    Reply Uninstall(const UninstallParamOption &paramOption);

    /**
     * @brief 更新软件包
     *
     * @param paramOption 更新包参数
     *
     * @return Reply 同Install
     */
    Reply Update(const ParamOption &paramOption);

    /**
     * @brief 查询软件包信息
     *
     * @param paramOption 查询命令参数
     *
     * @return QueryReply dbus方法调用应答 \n
     *         code 状态码 \n
     *         message 状态信息 \n
     *         result 查询结果
     */
    QueryReply Query(const QueryParamOption &paramOption);

public:
    QScopedPointer<QThreadPool> pool; ///< 下载、卸载、更新应用线程池

    /**
     * @brief 设置是否为nodbus安装模式
     *
     * @param enable true:nodbus false:dbus
     */
    void setNoDBusMode(bool enable);

private:
    QScopedPointer<PackageManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

protected:
    PackageManager();
    ~PackageManager() override;
};

} // namespace service
} // namespace linglong

#define POOL_MAX_THREAD 10 ///< 下载、卸载、更新应用线程池最大线程数
#define PACKAGE_MANAGER linglong::service::PackageManager::instance()
#endif
