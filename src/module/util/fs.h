/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QStringList>
#include <QString>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <stdlib.h>
#include <iostream>
//#include <libuossv/uossv.h>

namespace linglong {
namespace util {

QString jonsPath(const QStringList &component);

QString getUserFile(const QString &path);

QString ensureUserDir(const QStringList &relativeDirPathComponents);

bool inline fileExists(const QString &path)
{
    QFileInfo fs(path);
    return fs.exists() && fs.isFile() ? true : false;
}

bool inline dirExists(const QString &path)
{
    QFileInfo fs(path);
    return fs.exists() && fs.isDir() ? true : false;
}

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
    int ret = system(cmd.toStdString().c_str());

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
    int ret = system(cmd.toStdString().c_str());
    return true;
}

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
    int ret = system(cmd.toStdString().c_str());
    return true;
}

bool inline createDir(const QString &path)
{
    auto val = QDir().exists(path);
    if (!val) {
        return QDir().mkpath(path);
    }
    return true;
}

//sign uap-1 data.tgz
bool inline makeSign(QString data_input, QString certpath, QString key_path, QString &sign_data)
{/*
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

} // namespace util
} // namespace linglong
