/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

/*!
 * link "${targetDirPath}/${hostPath}/*" --> "${hostPrefix}/${hostPath}/*"
 * the mount namespace can chroot ${hostPrefix}, for example:
 *  ${targetDirPath}=/var/lib/linglong
 *  ${hostPath}=/usr
 *  ${virtual-base}=/virtual-base
 *  /usr/bin/bash will be relink like:
 *  "/var/lib/linglong/virtual-base/usr/bin/bash" --> "/virtual-base/usr/bin/bash"
 * @param hostPrefix
 * @param hostPath
 * @param targetDirPath
 * @return
 */
int generateBaseTo(const QString &hostPrefix, const QString &hostPath, const QString &targetDirPath)
{
    Q_ASSERT(QDir::isAbsolutePath(hostPrefix));
    Q_ASSERT(QDir::isAbsolutePath(hostPath));
    Q_ASSERT(QDir::isAbsolutePath(targetDirPath));

    QDir rootDir(hostPath);
    QDir targetDir(targetDirPath + hostPath);

    QDirIterator iter(rootDir.canonicalPath(),
                      QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                      (QDirIterator::Subdirectories));
    while (iter.hasNext()) {
        iter.next();
        auto relativePath = rootDir.relativeFilePath(iter.filePath());

        auto virtualSourceDir = QDir(hostPrefix + hostPath);
        auto virtualSource = virtualSourceDir.filePath(relativePath);
        auto sourcePath = iter.filePath();
        sourcePath = virtualSource;

        auto targetPath = targetDir.filePath(relativePath);
        QFileInfo fi(targetPath);
        qDebug() << "mkpath" << fi.path() << "for" << targetPath;
        QDir(fi.path()).mkpath(".");
        qDebug() << "link" << targetPath << "-->" << sourcePath;
        QFile::link(sourcePath, targetPath);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    generateBaseTo("/virtual-base", "/usr", "/persistent/linglong/virtual-base");
}