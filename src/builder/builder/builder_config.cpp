/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "builder_config.h"

#include "module/util/file.h"
#include "module/util/serialize/yaml.h"
#include "module/util/sysinfo.h"
#include "module/util/xdg.h"

namespace linglong {
namespace builder {

QString BuilderConfig::repoPath() const
{
    return QStringList{ util::userCacheDir().path(), "linglong-builder" }.join(QDir::separator());
}

QString BuilderConfig::ostreePath() const
{
    return QStringList{ util::userCacheDir().path(), "linglong-builder/repo" }.join("/");
}

QString BuilderConfig::targetFetchCachePath() const
{
    auto target =
            QStringList{ getProjectRoot(), ".linglong-target", getProjectName(), "fetch", "cache" }
                    .join("/");
    util::ensureDir(target);
    return target;
}

QString BuilderConfig::targetSourcePath() const
{
    auto target =
            QStringList{ getProjectRoot(), ".linglong-target", getProjectName(), "source" }.join(
                    "/");
    util::ensureDir(target);
    return target;
}

void BuilderConfig::setProjectRoot(const QString &path)
{
    projectRoot = path;
}

QString BuilderConfig::getProjectRoot() const
{
    return projectRoot;
}

void BuilderConfig::setProjectName(const QString &name)
{
    projectName = name;
}

QString BuilderConfig::getProjectName() const
{
    return projectName;
}

QString BuilderConfig::layerPath(const QStringList &subPathList) const
{
    QStringList list{ util::userCacheDir().path(), "linglong-builder/layers" };
    list.append(subPathList);
    return list.join(QDir::separator());
}

void BuilderConfig::setExec(const QString &execParam)
{
    exec = execParam;
}

QString BuilderConfig::getExec() const
{
    return exec;
}

QString BuilderConfig::templatePath() const
{
    for (auto dataPath : QStandardPaths::standardLocations(QStandardPaths::DataLocation)) {
        QString templatePath = QStringList{ dataPath, "template" }.join("/");
        if (util::dirExists(templatePath)) {
            return templatePath;
        }
    }
    return QString();
}

QString configPath()
{
    const QString filename = "linglong/builder.yaml";
    QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    configDirs.push_front("/etc");
    configDirs.push_front("/usr/local/etc");
    configDirs.push_front(QStringList{ QDir::currentPath(), ".." }.join(QDir::separator()));
    configDirs.push_front(QDir::currentPath());

    for (const auto &configDir : configDirs) {
        QString configPath = QStringList{ configDir, filename }.join(QDir::separator());
        if (QFile::exists(configPath)) {
            return configPath;
        }
    }

    return QString();
}

BuilderConfig *BuilderConfig::instance()
{
    if (!configPath().isEmpty()) {
        try {
            static auto config =
                    formYaml<BuilderConfig>(YAML::LoadFile(configPath().toStdString()));
            return config;
        } catch (std::exception &e) {
            qCritical() << e.what();
            qCritical().noquote() << QString("failed to parse builder.yaml");
        }
    }

    static BuilderConfig *cfg = new BuilderConfig();
    return cfg;
}

} // namespace builder
} // namespace linglong
