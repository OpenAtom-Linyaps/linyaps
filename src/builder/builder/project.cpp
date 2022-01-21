/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "project.h"

#include <QDebug>

#include "module/util/xdg.h"
#include "module/util/uuid.h"
#include "module/util/fs.h"
#include "module/util/sysinfo.h"
#include "module/package/ref.h"

#include "builder_config.h"

namespace linglong {
namespace builder {

const char *BuildScriptPath = "/entry.sh";
const char *BuildCacheDirectoryName = "linglong-builder";

const char *PackageKindApp = "app";
const char *PackageKindLib = "lib";
const char *PackageKindRuntime = "runtime";

class ProjectPrivate
{
public:
    explicit ProjectPrivate(Project *parent)
        : cfg(BuilderConfig::instance()->projectRoot(), parent)
        , q_ptr(parent)
    {
    }

    static QString buildArch() { return util::hostArch(); }

    package::Ref ref() const { return package::Ref("", q_ptr->package->id, q_ptr->package->version, buildArch()); }

    int generateBuildScript(const QString &path) const
    {
        QFile scriptFile(path);

        if (!scriptFile.open(QIODevice::WriteOnly)) {
            qCritical() << "failed to open" << path << scriptFile.error();
            return -1;
        }

        scriptFile.write("echo /runtime/lib >> /etc/ld.so.conf.d/runtime.conf\n");
        scriptFile.write("ldconfig\n");

        if (!BuilderConfig::instance()->exec().isEmpty()) {
            scriptFile.write(BuilderConfig::instance()->exec().toLocal8Bit() + "\n");
        }

        scriptFile.write(q_ptr->build->manual->configure.toLocal8Bit());
        scriptFile.close();

        return 0;
    }

    QString configFilePath; //! yaml path
    QString scriptPath;
    Project::Config cfg;
    Project *q_ptr = nullptr;
};

void Project::onPostSerialize()
{
    dd_ptr.reset(new ProjectPrivate(this));

    // TODO: how to define an build task
    auto cacheDirectory = config().cacheAbsoluteFilePath({"tmp"});
    util::ensureDir(cacheDirectory);
    auto scriptPath = cacheDirectory + BuildScriptPath;
    dd_ptr->generateBuildScript(scriptPath);
    dd_ptr->scriptPath = scriptPath;

    // get source.version form package.version
    if (source && source->version.isEmpty()) {
        source->version = package->version;
    }
}

QString Project::buildScriptPath() const
{
    return dd_ptr->scriptPath;
}

Project::Project(QObject *parent)
    : JsonSerialize(parent)
{
}

package::Ref Project::ref() const
{
    return dd_ptr->ref();
}

const Project::Config &Project::config() const
{
    return dd_ptr->cfg;
}

package::Ref Project::runtimeRef() const
{
    return fuzzyRef(runtime);
}

package::Ref Project::baseRef() const
{
    return fuzzyRef(base);
}

QString Project::configFilePath() const
{
    return dd_ptr->configFilePath;
}

void Project::setConfigFilePath(const QString &configFilePath)
{
    dd_ptr->configFilePath = configFilePath;
}

Project::~Project() = default;

QString Project::Config::rootPath() const
{
    return m_projectRoot;
}

QString Project::Config::absoluteFilePath(const QStringList &filenames) const
{
    auto targetList = QStringList {rootPath()};
    targetList.append(filenames);
    auto target = targetList.join("/");

    return target;
}

QString Project::Config::cacheAbsoluteFilePath(const QStringList &filenames) const
{
    auto targetList = QStringList {rootPath(), ".linglong-target"};
    targetList.append(filenames);
    auto target = targetList.join("/");
    return target;
}

QString Project::Config::cacheRuntimePath(const QString &subPath) const
{
    return cacheAbsoluteFilePath({"runtime", subPath});
}

QString Project::Config::cacheInstallPath(const QString &subPath) const
{
    if (PackageKindRuntime == m_project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    } else if (PackageKindLib == m_project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    } else if (PackageKindApp == m_project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    };
    return "";
}

QString Project::Config::targetArch() const
{
    return util::hostArch();
}

QString Project::Config::targetInstallPath(const QString &sub) const
{
    if (PackageKindRuntime == m_project->package->kind) {
        return QStringList {"/runtime", sub}.join("/");
    } else if (PackageKindLib == m_project->package->kind) {
        return QStringList {"/runtime", sub}.join("/");
    } else if (PackageKindApp == m_project->package->kind) {
        return QStringList {QString("/opt/apps/%1/files").arg(m_project->ref().appId), sub}.join("/");
    };
    return "";
}

} // namespace builder
} // namespace linglong