/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_PACKAGE_H_
#define LINGLONG_SRC_MODULE_PACKAGE_PACKAGE_H_

#include "module/util/qserializer/deprecated.h"
#include "ref.h"

#include <QDBusArgument>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QTemporaryDir>

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// 当前在线包对应的包名/版本/架构/uab存储URL信息
namespace linglong {
namespace package {

class AppMetaInfo : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(AppMetaInfo)

public:
    Q_JSON_PROPERTY(QString, appId);
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QString, arch);
    // app 类型
    Q_JSON_PROPERTY(QString, kind);
    Q_JSON_PROPERTY(QString, runtime);
    // 软件包对应的uab存储地址
    Q_JSON_PROPERTY(QString, uabUrl);
    // app 所属远端仓库名称
    Q_JSON_PROPERTY(QString, repoName);
    // app 描述
    Q_JSON_PROPERTY(QString, description);
    // 安装应用对应的用户
    Q_JSON_PROPERTY(QString, user);

    // app 大小
    Q_JSON_PROPERTY(QString, size);
    // app 渠道
    Q_JSON_PROPERTY(QString, channel);
    // app 版本类型 devel/release
    Q_JSON_PROPERTY(QString, module);

public:
    inline package::Ref ref() { return package::Ref("", appId, version, arch); }
};

} // namespace package
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::package, AppMetaInfo)

typedef QMap<QString, QString> ParamStringMap;

#endif
