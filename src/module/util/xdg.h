/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QDir>

namespace linglong {
namespace util {

QDir userCacheDir();

QDir userRuntimeDir();

QStringList parseExec(const QString &exec);

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


} // namespace util
} // namespace linglong
