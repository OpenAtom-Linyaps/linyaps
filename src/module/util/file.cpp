/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "file.h"

#include <sys/stat.h>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace linglong {
namespace util {

// 存储软件包信息服务器配置文件
const QString serverConfigPath = getLinglongRootPath() + "/config.json";

QString jonsPath(const QStringList &component)
{
    return QDir::toNativeSeparators(component.join(QDir::separator()));
}

QString getUserFile(const QString &path)
{
    auto dirPath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0);
    if (!path.isEmpty()) {
        dirPath += "/" + path;
    }
    return dirPath;
}
QString ensureUserDir(const QString &relativeDirPath)
{
    QStringList dirPathComponents = {
        QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0),
        relativeDirPath,
    };
    auto dirPath = QDir::cleanPath(dirPathComponents.join(QDir::separator()));
    QDir(dirPath).mkpath(".");
    return dirPath;
}

QString ensureUserDir(const QStringList &relativeDirPathComponents)
{
    auto relativeFilepath = QDir::cleanPath(relativeDirPathComponents.join(QDir::separator().toLatin1()));
    return ensureUserDir(relativeFilepath);
}

bool ensureDir(const QString &path)
{
    QDir dir(path);
    dir.mkpath(".");
    return true;
}

QString createProxySocket(const QString &pattern)
{
    auto userRuntimeDir = QString("/run/user/%1/").arg(getuid());
    QString socketDir = userRuntimeDir + ".dbus-proxy/";
    bool ret = util::createDir(socketDir);
    if (!ret) {
        qCritical() << "createProxySocket pattern:" << pattern << " failed";
        return QString();
    }
    QTemporaryFile tmpFile(socketDir + pattern);
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        qCritical() << "create " << socketDir + pattern << " failed";
        return QString();
    }
    tmpFile.close();
    return tmpFile.fileName();
}

/*
 * 从配置文件获取服务器配置参数
 *
 * @param key: 参数名称
 * @param value: 查询结果
 *
 * @return int: 0:成功 其它:失败
 */
int getLocalConfig(const QString &key, QString &value)
{
    if (!linglong::util::fileExists(serverConfigPath)) {
        qCritical() << serverConfigPath << " not exist";
        return STATUS_CODE(kFail);
    }
    QFile dbFile(serverConfigPath);
    dbFile.open(QIODevice::ReadOnly);
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (QJsonParseError::NoError != parseJsonErr.error) {
        qCritical() << "parse linglong config file err";
        return STATUS_CODE(kFail);
    }
    QJsonObject dataObject = document.object();
    if (!dataObject.contains(key)) {
        qWarning() << "key:" << key << " not found in config";
        return STATUS_CODE(kFail);
    }
    value = dataObject[key].toString();
    return STATUS_CODE(kSuccess);
}

QStringList getUserInfo()
{
    auto filePath = util::getUserFile(".linglong/.user.json");
    QStringList userInfo;

    QTextStream qin(stdin, QIODevice::ReadOnly);

    QFile file(filePath);

    if (file.exists()) {
        file.open(QIODevice::ReadOnly);

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject infoObj = doc.object();

        userInfo.append(infoObj["username"].toString());
        userInfo.append(infoObj["password"].toString());

        file.close();
    } else {
        QString name;
        QString password;

        qInfo() << "please enter ldap account: ";
        qin >> name;
        qInfo() << "please enter password: ";
        qin >> password;

        userInfo.append(name);
        userInfo.append(password);
    }

    return userInfo;
}

quint64 sizeOfDir(const QString &srcPath)
{
    QDir srcDir(srcPath);
    quint64 size = 0;
    QFileInfoList list = srcDir.entryInfoList();

    for (auto info : list) {
        if (info.fileName() == "." || info.fileName() == "..") {
            continue;
        }
        if (info.isSymLink()) {
            // FIXME: https://bugreports.qt.io/browse/QTBUG-50301
            struct stat symlinkStat;
            lstat(info.absoluteFilePath().toLocal8Bit(), &symlinkStat);
            size += symlinkStat.st_size;
        } else if (info.isDir()) {
            // 一个文件夹大小为4K
            size += 4 * 1024;
            size += sizeOfDir(QStringList {srcPath, info.fileName()}.join("/"));
        } else {
            size += info.size();
        }
    }

    return size;
}

QString fileHash(const QString &path, QCryptographicHash::Algorithm method)
{
    QFile sourceFile(path);
    qint64 fileSize = sourceFile.size();
    const qint64 bufferSize = 2 * 1024 * 1024;

    if (sourceFile.open(QIODevice::ReadOnly)) {
        char buffer[bufferSize];
        int bytesRead;
        int readSize = qMin(fileSize, bufferSize);

        QCryptographicHash hash(method);

        while (readSize > 0 && (bytesRead = sourceFile.read(buffer, readSize)) > 0) {
            fileSize -= bytesRead;
            hash.addData(buffer, bytesRead);
            readSize = qMin(fileSize, bufferSize);
        }

        sourceFile.close();
        return QString(hash.result().toHex());
    }

    return QString();
}

} // namespace util
} // namespace linglong
