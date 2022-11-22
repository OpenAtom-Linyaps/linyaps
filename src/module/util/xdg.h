/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_XDG_H_
#define LINGLONG_SRC_MODULE_UTIL_XDG_H_

#include <QDir>
#include <QString>

namespace linglong {
namespace util {

QDir userCacheDir();

QDir userRuntimeDir();

QStringList parseExec(const QString &exec);

QStringList splitExec(const QString &exec);

/*!
 * parseEnv will convert ${HOME}/Desktop:${HOME}/Desktop to real value as pair
 * /home/linglong/Desktop, /home/linglong/Desktop
 * support format:
 * ${HOME}/Desktop:${HOME}/Desktop
 * ${HOME}/Desktop=${HOME}/Desktop
 * @return
 */
QPair<QString, QString> parseEnvKeyValue(QString env, const QString &sep);

//转换参数中的特殊字符
//参数中的空格与”加上双斜杠
QStringList convertSpecialCharacters(const QStringList &args);

/*
 * \brief getXdgDir get user dir by xdg dir name
 * \param filename xdg file name
 * \return bool/QString 成功返回true，失败返回false 和目录路径 ,若为false，则返回空字符串，目录路径不存在，则返回false
 *
 */
QPair<bool, QString> getXdgDir(QString name);

/*!
 * \brief getXdgUserDir get user dir by xdg key list
 * \return QList<QString> 目录列表
 *
 */
QList<QString> getXdgUserDir();

/*!
 * \brief getPathInXdgUserConfig  get xdg key in config file
 * \return QString xdg path 如果为空，表示没有找到
 *
 */
QString getPathInXdgUserConfig(const QString &key);

} // namespace util
} // namespace linglong
#endif
