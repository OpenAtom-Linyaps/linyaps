/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "builder_config.h"

#include "module/util/xdg.h"
#include "module/util/file.h"
#include "module/util/sysinfo.h"

namespace linglong {
namespace builder {

QString BuilderConfig::repoPath() const
{
    return QStringList {util::userCacheDir().path(), "linglong-builder"}.join(QDir::separator());
}

QString BuilderConfig::ostreePath() const
{
    return QStringList {util::userCacheDir().path(), "linglong-builder/repo"}.join("/");
}

QString BuilderConfig::targetFetchCachePath() const
{
    auto target = QStringList {projectRoot(), ".linglong-target", projectName(), "fetch", "cache"}.join("/");
    util::ensureDir(target);
    return target;
}

QString BuilderConfig::targetSourcePath() const
{
    auto target = QStringList {projectRoot(), ".linglong-target", projectName(),"source"}.join("/");
    util::ensureDir(target);
    return target;
}

void BuilderConfig::setProjectRoot(const QString &path)
{
    m_projectRoot = path;
}

QString BuilderConfig::projectRoot() const
{
    return m_projectRoot;
}

void BuilderConfig::setProjectName(const QString &name)
{
    m_projectName = name;
}

QString BuilderConfig::projectName() const
{
    return m_projectName;
}

QString BuilderConfig::layerPath(const QStringList &subPathList) const
{
    QStringList list {util::userCacheDir().path(), "linglong-builder/layers"};
    list.append(subPathList);
    return list.join(QDir::separator());
}

void BuilderConfig::setExec(const QString &exec)
{
    m_exec = exec;
}

QString BuilderConfig::exec() const
{
    return m_exec;
}

QString BuilderConfig::templatePath() const
{
    for (auto dataPath : QStandardPaths::standardLocations(QStandardPaths::DataLocation)) {
        QString templatePath = QStringList {dataPath, "template"}.join("/");
        if (util::dirExists(templatePath)) {
            return templatePath;
        }
    }
    return QString();
}

} // namespace builder
} // namespace linglong