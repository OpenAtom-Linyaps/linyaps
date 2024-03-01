/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/builder_config.h"

#include "linglong/util/file.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/xdg.h"

#include <mutex>

static void initQResource()
{
    Q_INIT_RESOURCE(builder_releases);
}

namespace linglong {
namespace builder {
namespace PrivateBuilderConfigInit {

int init()
{
    static std::once_flag flag;
    std::call_once(flag, initQResource);
    return 0;
}
} // namespace PrivateBuilderConfigInit

QSERIALIZER_IMPL(BuilderConfig)

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
      QStringList{ getProjectRoot(), ".linglong-target", getProjectName(), "fetch", "cache" }.join(
        "/");
    util::ensureDir(target);
    return target;
}

QString BuilderConfig::targetSourcePath() const
{
    auto target =
      QStringList{ getProjectRoot(), ".linglong-target", getProjectName(), "source" }.join("/");
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

void BuilderConfig::setExec(const QStringList &execParam)
{
    exec = execParam;
}

QStringList BuilderConfig::getExec() const
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

void BuilderConfig::setBuildArch(const QString &arch)
{
    buildArch = arch;
}

QString BuilderConfig::getBuildArch() const
{
    if (buildArch.isEmpty()) {
        return linglong::util::hostArch();
    }
    return buildArch;
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

void BuilderConfig::setBuildEnv(const QStringList &env)
{
    buildEnv = env;
}

QStringList BuilderConfig::getBuildEnv() const
{
    return buildEnv;
}

static QStringList projectConfigPaths()
{
    QStringList result{};

    auto pwd = QDir::current();

    do {
        auto configPath =
          QStringList{ pwd.absolutePath(), ".ll-builder", "config.yaml" }.join(QDir::separator());
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
    static QSharedPointer<BuilderConfig> cfg;
    static std::once_flag flag;
    std::call_once(flag, []() {
        util::Error err;
        const auto configPath = getConfigPath();
        qDebug() << "load config from" << configPath;
        try {
            std::tie(cfg, err) = util::fromYAML<QSharedPointer<BuilderConfig>>(configPath);
        } catch (std::exception &e) {
            qCritical().noquote()
              << QString("Failed to parse config [%1]: %2").arg(configPath, e.what());
            cfg.reset(new BuilderConfig());
            return;
        }
    });

    return cfg.data();
}

} // namespace builder
} // namespace linglong
