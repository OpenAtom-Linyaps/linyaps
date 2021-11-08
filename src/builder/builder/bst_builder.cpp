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

#include "module/package/package.h"
#include "module/util/fs.h"
#include "module/util/yaml.h"

#include "project.h"

namespace linglong {
namespace builder {

static auto projectConfTemplate = R"PCT0(
# Unique project name
name: %1
id: %2

# Required BuildStream format version
format-version: 17

# Subdirectory where elements are stored
element-path: elements
)PCT0";

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

    templateDirCopy(":org.deepin.demo", projectName, {"project.conf", "elements/export.bst", "files/loader.sh"},
                    {
                        {"PROJECT_NAME", bstName},
                        {"APP_ID", projectName},
                    });

    auto hint =
        QString("run `cd %1 && %2 build` to build project").arg(projectName, QCoreApplication::applicationFilePath());

    std::cout << hint.toStdString();

    return dResultBase();
}

util::Result BstBuilder::Build()
{
    QProcess process;
    process.setProgram("bst");

    QStringList args = {"build"};
    process.setArguments(args);

    QProcess::connect(&process, &QProcess::readyReadStandardOutput,
                      [&]() { std::cout << process.readAllStandardOutput().toStdString().c_str(); });

    QProcess::connect(&process, &QProcess::readyReadStandardError,
                      [&]() { std::cout << process.readAllStandardError().toStdString().c_str(); });

    process.start();
    process.waitForStarted(-1);
    process.waitForFinished(-1);

    return dResultBase() << process.exitCode() << process.errorString();
}

util::Result BstBuilder::Push(const QString &repoURL, bool force)
{
    // TODO: push build result to repoURL
    return dResultBase();
}

util::Result BstBuilder::Export(const QString &outputFilepath)
{
    // TODO: export build result to uab package
    auto project = formYaml<Project>(YAML::LoadFile("project.conf"));

    auto id = project->id;

    qDebug() << "export " << id << "to bundle file";

    // TODO: fix runner problem
    QProcess process;
    process.setProgram("bst");

    QStringList args = {"checkout", "export", "export"};

    process.setArguments(args);

    QProcess::connect(&process, &QProcess::readyReadStandardOutput,
                      [&]() { std::cout << process.readAllStandardOutput().toStdString().c_str(); });

    QProcess::connect(&process, &QProcess::readyReadStandardError,
                      [&]() { std::cout << process.readAllStandardError().toStdString().c_str(); });

    process.start();
    process.waitForStarted(-1);
    process.waitForFinished(-1);

    // TODO: package export to uab

    return dResultBase();
}

} // namespace builder
} // namespace linglong