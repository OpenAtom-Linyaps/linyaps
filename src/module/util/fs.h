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

#include <stdlib.h>
#include <iostream>
#include <QStringList>
#include <QString>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QDirIterator>
#include <QDebug>

namespace linglong {
namespace util {

QString jonsPath(const QStringList &component);

QString getUserFile(const QString &path);

QString ensureUserDir(const QStringList &relativeDirPathComponents);

/*!
 * 判断文件是否存在
 * @param path
 * @return
 */
bool inline fileExists(const QString &path)
{
    QFileInfo fs(path);
    return fs.exists() && fs.isFile() ? true : false;
}

/*!
 * 判断目录是否存在
 * @param path
 * @return
 */
bool inline dirExists(const QString &path)
{
    QFileInfo fs(path);
    return fs.exists() && fs.isDir() ? true : false;
}

/*!
 * 压缩data.tgz文件
 * @param src
 * @param dest
 * @return
 */
bool inline makeData(const QString &src, QString &dest)
{
    QFileInfo fs1(src);

    char temp_prefix[1024] = "/tmp/uap-XXXXXX";
    char *dir_name = mkdtemp(temp_prefix);
    QFileInfo fs2(dir_name);

    if (!fs1.exists() || !fs2.exists()) {
        return false;
    }
    dest = QString::fromStdString(dir_name) + "/data.tgz";
    QString cmd = "tar -C " + src + " -cf - . | gzip --rsyncable >" + dest;
    // std::cout << "cmd:" << cmd.toStdString() << std::endl;
    // TODO:(FIX) ret value check
    system(cmd.toStdString().c_str());

    return true;
}

bool inline extractUap(const QString &uapfile, QString &dest)
{
    QFileInfo fs1(uapfile);

    char temp_prefix[1024] = "/tmp/uap-XXXXXX";
    char *dir_name = mkdtemp(temp_prefix);
    QFileInfo fs2(dir_name);

    if (!fs1.exists() || !fs2.exists()) {
        return false;
    }
    dest = QString::fromStdString(dir_name);
    QString cmd = "tar -xf " + uapfile + " -C " + dest;
    // TODO:(FIX) ret value check
    std::cout << "cmd:" << cmd.toStdString() << std::endl;
    system(cmd.toStdString().c_str());
    return true;
}

/*!
 * 解压uap文件
 * @param uapfile uap文件
 * @param dest 解压路径
 * @return bool
 */
bool inline extractUapData(const QString &uapfile, QString &dest)
{
    QFileInfo fs1(uapfile);
    QFileInfo fs2(dest);

    if (!fs1.exists() || !fs2.exists()) {
        return false;
    }
    QString cmd = "tar -xf " + uapfile + " -C " + dest;
    // TODO:(FIX) ret value check
    std::cout << "cmd:" << cmd.toStdString() << std::endl;
    system(cmd.toStdString().c_str());
    return true;
}

/*!
 * 创建目录
 * @param path 路径
 * @return bool
 */
bool inline createDir(const QString &path)
{
    auto val = QDir().exists(path);
    if (!val) {
        return QDir().mkpath(path);
    }
    return true;
}

/*!
 * 移除目录
 * @param path 路径
 * @return bool
 */
bool inline removeDir(const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        return dir.removeRecursively();
    }

    return true;
}

/*!
 * 获取指定目录下的子目录
 * @param path 输入路径
 * @param subdir 探测子目录，默认false
 * @return QStringList
 */
QStringList inline listDirFolders(const QString &path, const bool subdir = false)
{
    QStringList parent;

    QDirIterator dirs(path, QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                      subdir ? (QDirIterator::IteratorFlag)0x2 : (QDirIterator::IteratorFlag)0x0);

    while (dirs.hasNext()) {
        dirs.next();
        parent << dirs.filePath();
    }
    return parent;
}

/*!
 * 制作data.tgz签名
 * @param data_input
 * @param certpath
 * @param key_path
 * @param sign_data
 * @return bool
 */
bool inline makeSign(QString data_input, QString certpath, QString key_path, QString &sign_data)
{ /*
QString input = data_input;
std::string input_str = input.toStdString();
//certpath key_path to GoString cert and GoString key
GoSlice data = {
.data = (void *)input_str.c_str(),
.len = input_str.length(),
.cap = input_str.length(),
};
std::string priv_crt = certpath.toStdString();
std::string priv_key = key_path.toStdString();
GoString cert = {
.p = priv_crt.c_str(),
.n = priv_crt.length()};
GoString key = {
.p = priv_key.c_str(),
.n = priv_key.length()};

GoUint8 noTSA = 0; //time stamp 0 no   1 yes
GoUint8 timeout = 0;
GoSlice output;
int ret = (int)UOSSign(data, cert, key, noTSA, timeout, &output);
if (ret == 0) {
sign_data = ((char *)output.data);
return true;
} else {
return false;
}
*/
    return true;
}

/*!
 * 签名验证
 * @param data_input
 * @param sign_data_Q
 * @return
 */
bool inline checkSign(QString data_input, QString sign_data_Q)
{
    /*
QString input = data_input;
std::string input_str = input.toStdString();
std::string sign_data = sign_data_Q.toStdString();
GoSlice data = {
.data = (void *)input_str.c_str(),
.len = input_str.length(),
.cap = input_str.length(),
};
GoSlice sign = {
.data = (void *)sign_data.c_str(),
.len = sign_data.length(),
.cap = sign_data.length(),
};
GoUint8 dump = 0; //print cert 0 no   1  yes
int ret = (int)UOSVerify(data, sign, dump);
if (ret == 0) {
return true;
} else {
return false;
}
*/
    return true;
}

/*!
 * 建立link
 * @param src 来源
 * @param dest 目标
 * @param override 默认覆盖
 * @return
 */
bool inline linkFile(const QString &src, const QString &dest, const bool override = true)
{
    // QFile::link(const QString &fileName, const QString &linkName)
    QFile::link(src, dest);
    return true;
}

/*!
 * 拷贝目录
 * @param src 来源
 * @param dst 目标
 * @return
 */
void inline copyDir(const QString &src, const QString &dst)
{
    QDir srcDir(src);
    QDir dstDir(dst);

    if (!dstDir.exists()) {
        dstDir.mkpath(".");
    }

    QFileInfoList list = srcDir.entryInfoList();

    foreach (QFileInfo info, list) {
        if (info.fileName() == "." || info.fileName() == "..") {
            continue;
        }
        if (info.isDir()) {
            // 穿件文件夹，递归调用
            copyDir(info.filePath(), dst + "/" + info.fileName());
            continue;
        }
        // 拷贝文件
        QFile file(info.filePath());
        file.copy(dst + "/" + info.fileName());
    }
}

} // namespace util
} // namespace linglong
