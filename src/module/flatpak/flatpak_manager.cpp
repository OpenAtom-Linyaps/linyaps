/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "flatpak_manager.h"

#include <QJsonParseError>
#include <QJsonObject>
#include <QStringList>

#include "module/util/file.h"
#include "module/util/sysinfo.h"

namespace linglong {
namespace flatpak {

static QString getUserFlatpakDataDir()
{
    static QString kUserFlatpakDataDir;
    if (kUserFlatpakDataDir.isEmpty()) {
        kUserFlatpakDataDir = QString(qgetenv("XDG_DATA_HOME")) + "/flatpak/";
    }
    return kUserFlatpakDataDir;
}

QStringList findFileList(const QString &startPath, const QStringList &filter)
{
    QStringList fileName;
    QDir dir(startPath);
    // 搜索当前目录符合条件的文件
    foreach (QString file, dir.entryList(filter, QDir::Files)) {
        fileName += startPath + '/' + file;
    }
    // 搜索当前目录的子目录符合条件的文件
    foreach (QString subDir, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        // 过滤掉export目录
        if (subDir == "export") {
            continue;
        }
        fileName += findFileList(startPath + '/' + subDir, filter);
    }
    return fileName;
}

QString FlatpakManager::getRuntimePath(const QString &appId)
{
    auto manifestPath =
        QStringList {getUserFlatpakDataDir(), "app", appId, "current/active/files/manifest.json"}.join("/");

    if (!util::fileExists(manifestPath)) {
        qCritical() << manifestPath << " is not exist";
        return QString();
    }

    QFile jsonFile(manifestPath);
    jsonFile.open(QIODevice::ReadOnly);

    QJsonParseError parseJsonErr {};
    QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &parseJsonErr);
    jsonFile.close();
    if (parseJsonErr.error != QJsonParseError::NoError) {
        qCritical() << "flatpak parse manifest.json err" << parseJsonErr.errorString();
        return QString();
    }

    QJsonObject jsonObject = document.object();
    if (!jsonObject.contains("runtime") || !jsonObject.contains("runtime-version")) {
        qCritical() << "can not found key runtime or runtime-version";
        return QString();
    }

    QString runtimeId = jsonObject["runtime"].toString();
    QString runtimeVersion = jsonObject["runtime-version"].toString();
    QString arch = util::hostArch();
    // runtime/org.gnome.Platform/x86_64/41
    QString runtimePath = QString("runtime/%1/%2/%3").arg(runtimeId, arch, runtimeVersion);
    return QStringList {getUserFlatpakDataDir(), runtimePath, "active/files"}.join("/");
}

QString FlatpakManager::getAppPath(const QString &appId)
{
    return QStringList {getUserFlatpakDataDir(), "app", appId, "current/active/files"}.join("/");
}

QStringList FlatpakManager::getAppDesktopFileList(const QString &appId)
{
    return findFileList(getUserFlatpakDataDir() + "app/" + appId, {"*.desktop"});
}

} // namespace flatpak
} // namespace linglong