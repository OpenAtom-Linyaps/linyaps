/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "file.h"

#include "linglong/utils/configure.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <sys/stat.h>

namespace linglong::util {

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
    auto relativeFilepath =
      QDir::cleanPath(relativeDirPathComponents.join(QDir::separator().toLatin1()));
    return ensureUserDir(relativeFilepath);
}

bool ensureDir(const QString &path)
{
    QDir dir(path);
    return dir.mkpath(".");
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
            size += sizeOfDir(QStringList{ srcPath, info.fileName() }.join("/"));
        } else {
            size += info.size();
        }
    }

    return size;
}

QString fileHash(QIODevice &device, QCryptographicHash::Algorithm method)
{
    qint64 fileSize = device.size();
    const qint64 bufferSize = 2 * 1024 * 1024;

    char buffer[bufferSize];
    int bytesRead;
    int readSize = qMin(fileSize, bufferSize);

    QCryptographicHash hash(method);

    while (readSize > 0 && (bytesRead = device.read(buffer, readSize)) > 0) {
        fileSize -= bytesRead;
        hash.addData(buffer, bytesRead);
        readSize = qMin(fileSize, bufferSize);
    }

    return QString(hash.result().toHex());
}

QString fileHash(const QString &path, QCryptographicHash::Algorithm method)
{
    QFile sourceFile(path);
    if (sourceFile.open(QIODevice::ReadOnly)) {
        auto hash = fileHash(sourceFile, method);
        sourceFile.close();
        return hash;
    }
    return QString();
}

QString findLinglongConfigPath(const QString &subpath, bool writeable)
{

    auto writeablePath = QDir(getLinglongRootPath()).absoluteFilePath(subpath);
    auto paths = QList{ writeablePath, QDir(LINGLONG_DATA_DIR).absoluteFilePath(subpath) };

    if (writeable) {
        return writeablePath;
    }

    for (const auto &path : paths) {
        if (!QFile(path).exists()) {
            continue;
        }
        return path;
    }

    return "";
}

} // namespace linglong::util
