/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bst_builder.h"

#include <iostream>

#include <QCoreApplication>
#include <QProcess>
#include <QDir>

#include "module/util/fs.h"
#include "module/util/yaml.h"

#include "project.h"

namespace linglong {
namespace builder {

util::Result templateDirCopy(const QString &srcDir, const QString &destDir, const QStringList &replaceFilenameList,
                             QMap<QString, QString> variables)
{
    util::copyDir(srcDir, destDir);

    QDir dest(destDir);

    foreach (auto filename, replaceFilenameList) {
        auto filepath = dest.absoluteFilePath(filename);
        QFile templateFile(filepath);
        templateFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

        templateFile.open(QIODevice::ReadOnly);

        auto templateData = templateFile.readAll();
        foreach (auto const &k, variables.keys()) {
            templateData.replace(QString("@%1@").arg(k).toLocal8Bit(), variables.value(k).toLocal8Bit());
        }
        templateFile.close();

        templateFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        templateFile.write(templateData);
        templateFile.close();
    }

    return dResultBase();
}

util::Result BstBuilder::create(const QString &projectName)
{
    auto bstName = QString(projectName).replace(".", "-");

    // TODO: can read list from file
    auto replaceFilenameList = QStringList {"project.conf",
                                            "files/resource/uap.json",
                                            "files/resource/loader",
                                            "files/resource/info.json",
                                            "files/resource/entries/applications/org.deepin.demo.desktop",
                                            "files/src/mainwindow.cpp"};
    templateDirCopy(":org.deepin.demo", projectName, replaceFilenameList,
                    {
                        {"PROJECT_NAME", bstName},
                        {"APP_ID", projectName},
                    });

    //设置loader、demo 、org.deepin.demo.desktop运行权限
    QFile(QDir(projectName).absoluteFilePath("files/resource/loader"))
        .setPermissions(QFileDevice::ExeOwner | QFileDevice::WriteOwner | QFileDevice::ReadOwner);
    QFile(QDir(projectName).absoluteFilePath("files/resource/entries/applications/org.deepin.demo.desktop"))
        .setPermissions(QFileDevice::ExeOwner | QFileDevice::WriteOwner | QFileDevice::ReadOwner);

    auto hint =
        QString("run `cd %1 && %2 build` to build project").arg(projectName, QCoreApplication::applicationFilePath());

    std::cout << hint.toStdString() << std::endl;

    return dResultBase();
}

util::Result BstBuilder::build()
{
    return linglong::package::runner("bst", {"build", "export.bst"});
}

util::Result BstBuilder::exportBundle(const QString &outputFilePath)
{
    auto project = formYaml<Project>(YAML::LoadFile("project.conf"));

    if (!project->variables) {
        return dResultBase() << -1 << "bst project.conf must contains variables.id and it value should be appid";
    }

    auto id = project->variables->id;

    QString dataDir = QString("export");
    if (dirExists(dataDir)) {
        removeDir(dataDir);
    }

    qDebug() << "export " << id << "to bundle file";
    // checkout 应用包
    auto ret = linglong::package::runner("bst", {"checkout", "export.bst", dataDir});
    if (!ret.success()) {
        return dResult(ret) << "call bst checkout failed";
    }

    // FIXME: export build result to uab package
    //开始制作uab
    linglong::package::Bundle uabBundle;
    auto makeBundleResult = uabBundle.make(dataDir, outputFilePath);
    if (!makeBundleResult.success()) {
        return dResult(makeBundleResult) << "make bundle failed!!!";
    }

    return dResultBase();
}

// util::Result BstBuilder::push(const QString &repoURL, bool force)
util::Result BstBuilder::push(const QString &bundleFilePath, bool force)
{
    // TODO: push build result to repoURL
    std::cout << "start upload ..." << std::endl;
    linglong::package::Bundle uabBundle;
    auto pushBundleResult = uabBundle.push(bundleFilePath, force);
    if (!pushBundleResult.success()) {
        return dResult(pushBundleResult) << "push bundle failed!!!";
    }
    return dResultBase();
}

} // namespace builder
} // namespace linglong
