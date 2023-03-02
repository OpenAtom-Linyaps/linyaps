/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "builder_config.h"

#include "configure.h"
#include "module/util/file.h"
#include "module/util/serialize/yaml.h"
#include "module/util/sysinfo.h"
#include "module/util/xdg.h"

#include <mutex>

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

void BuilderConfig::setOffline(const bool &offlineParam)
{
    offline = offlineParam;
}

bool BuilderConfig::getOffline() const
{
    return offline;
}

QString BuilderConfig::templatePath() const
{
    for (auto dataPath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        QString templatePath =
                QStringList{ dataPath, "linglong", "builder", "templates" }.join(QDir::separator());
        if (util::dirExists(templatePath)) {
            return templatePath;
        }
    }
    return QString();
}

static QStringList projectConfigPaths()
{
    QStringList result{};

    auto pwd = QDir::current();

    do {
        auto configPath = QStringList{ pwd.absolutePath(), ".ll-builder", "config.yaml" }.join(
                QDir::separator());
        result << std::move(configPath);
    } while (pwd.cdUp());

    return result;
}

static QStringList nonProjectConfigPaths()
{
    QStringList result{};

    auto configLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    configLocations.append(SYSCONFDIR);

    for (const auto &configLocation : configLocations) {
        result << QStringList{ configLocation, "linglong", "builder", "config.yaml" }.join(
                QDir::separator());
    }

    result << QStringList{ DATADIR, "linglong", "builder", "config.yaml" }.join(QDir::separator());

    return result;
}

static QString getConfigPath()
{
    QStringList configPaths = {};

    configPaths << projectConfigPaths();
    configPaths << nonProjectConfigPaths();

    qDebug() << "Searching ll-builder config in:" << configPaths;

    for (const auto &configPath : configPaths) {
        if (QFile::exists(configPath)) {
            return configPath;
        }
    }

    return ":/config.yaml";
}

BuilderConfig *BuilderConfig::instance()
{
    static BuilderConfig *cfg;
    static std::once_flag flag;

    std::call_once(flag, []() {
        const auto configPath = getConfigPath();
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            qCritical().noquote() << QString("Failed to open config file [%1]")
                                             .arg(configPath.startsWith(":") ? "builtin"
                                                                             : configPath);
            cfg = new BuilderConfig();
            return;
        }

        try {
            cfg = formYaml<BuilderConfig>(YAML::Load(configFile.readAll().data()));
        } catch (std::exception &e) {
            qCritical().noquote() << QString("Failed to parse config [%1]: %2")
                                             .arg(configPath.startsWith(":") ? "builtin"
                                                                             : configPath)
                                             .arg(e.what());
            cfg = new BuilderConfig();
            return;
        }
    });

    return cfg;
}

} // namespace builder
} // namespace linglong
