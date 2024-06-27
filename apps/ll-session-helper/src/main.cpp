/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <QCoreApplication>
#include <QObject>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QTimer>
#include <unistd.h>

void copyFileToTargetPath(const QFileInfo &fileInfo, const QString path);
void copyDirToTargetPath(const QFileInfo &fileInfo, const QString path, QMap<QString, QMap<QString, QDateTime>> &directoryFileTimes);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QFileSystemWatcher *qFileSystemWatcher = new QFileSystemWatcher();
    QStringList watchFilesPaths = {"/etc/localtime", "/etc/resolv.conf", "/etc/timezone"};
    QMap<QString, QMap<QString, QDateTime>> directoryFileTimes;;  // 存储每个目录及其目录下对应的文件修改时间映射
    QString uidString = QString::number(getuid());
    QDir monitorPath = QStringLiteral("/run/user/") + uidString + QStringLiteral("/linglong/monitor");

    if (!monitorPath.exists())
        monitorPath.mkpath(".");

    for(QString &filePath: watchFilesPaths) {
        qDebug() << QString("Add to watch: %1").arg(filePath);
        qFileSystemWatcher->addPath(filePath);

        // 初始化，先做一次拷贝到 monitor 目录下
        QFileInfo fileInfo(filePath);
        if (fileInfo.isFile()) {
            copyFileToTargetPath(fileInfo, monitorPath.path());
        } else {
            copyDirToTargetPath(fileInfo, monitorPath.path(), directoryFileTimes);
        }
    }

    QObject::connect(qFileSystemWatcher, &QFileSystemWatcher::fileChanged, [&](const QString &path){
        QFileInfo fileInfo(path);
        QString filePath = fileInfo.absolutePath();

        qDebug() << QString("The file %1 at path %2 is updated").arg(fileInfo.fileName()).arg(filePath);

        copyFileToTargetPath(fileInfo, monitorPath.path());

        // 移动或者重命名都会被移除监听，需要重新加回
        qFileSystemWatcher->addPath(path);

    });

    QObject::connect(qFileSystemWatcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path){
        QFileInfo fileInfo(path);
        copyDirToTargetPath(fileInfo, monitorPath.path(), directoryFileTimes);
    });

    return a.exec();
}

// 拷贝修改的文件到目标路径，同时文件的父目录也需要携带上
void copyFileToTargetPath(const QFileInfo &fileInfo, const QString &path) {
    QString targetFilePath = path + "/" + fileInfo.fileName();

    if (QFile::exists(targetFilePath)) {
        qDebug() << targetFilePath << "is exists, remove file";
        QFile::remove(targetFilePath);
    }

    if (QFile::copy(fileInfo.filePath(), targetFilePath)) {
        qDebug() << fileInfo.fileName() << "copied successfully to" << targetFilePath;
    } else {
        qDebug() << "Failed to copy " << fileInfo.fileName() << "to" << targetFilePath;
    }
}

// 拷贝当前目录及目录下的文件，不包括子目录
void copyDirToTargetPath(const QFileInfo &fileInfo, const QString &path, QMap<QString, QMap<QString, QDateTime>> &directoryFileTimes) {
    QFileInfoList fileList = QDir(fileInfo.filePath()).entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    QMap<QString, QDateTime> &fileTimeMap = directoryFileTimes[fileInfo.path()];

    for (const QFileInfo &file: fileList) {
        QDateTime lastModified = file.lastModified().toLocalTime();

        // .swp, ~ 是编辑过程中产生的临时文件，不要触发修改文件的操作。
        if (file.fileName().endsWith(".swp") || file.fileName().endsWith("~")) continue;

        // 在 fileTimeMap 找到对应的文件的时间戳，并且修改时间大于记录的时间说明被修改了，或者没有被记录的文件（第一次初始化）
        if ((fileTimeMap.contains(file.fileName()) && lastModified > fileTimeMap[file.fileName()]) || !fileTimeMap.contains(file.fileName())) {
            qDebug() << file.fileName() << "in directory" << path << "was modified or add";

            copyFileToTargetPath(file, path);

            fileTimeMap[file.fileName()] = lastModified;
        }
    }
}
