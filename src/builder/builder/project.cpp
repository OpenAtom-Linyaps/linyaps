/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "project.h"

#include <QDebug>

#include "module/util/xdg.h"
#include "module/util/uuid.h"
#include "module/util/file.h"
#include "module/util/sysinfo.h"
#include "module/util/serialize/yaml.h"
#include "module/package/ref.h"

#include "builder_config.h"

namespace linglong {
namespace builder {

const char * const DependTypeRuntime = "runtime";

const char * const BuildScriptPath = "/entry.sh";
const char * const BuildCacheDirectoryName = "linglong-builder";

const char * const PackageKindApp = "app";
const char * const PackageKindLib = "lib";
const char * const PackageKindRuntime = "runtime";

class ProjectPrivate
{
public:
    explicit ProjectPrivate(Project *parent)
        : cfg(BuilderConfig::instance()->getProjectRoot(), parent)
        , q_ptr(parent)
    {
    }

    static QString buildArch() { return util::hostArch(); }

    package::Ref ref() const { return package::Ref("", q_ptr->package->id, q_ptr->package->version, buildArch()); }

    package::Ref fullRef(const QString &channel, const QString &module) const
    {
        return package::Ref("", channel, q_ptr->package->id, q_ptr->package->version, buildArch(), module);
    }

    package::Ref refWithModule(const QString &module) const
    {
        return package::Ref("", q_ptr->package->id, q_ptr->package->version, buildArch(), module);
    }

    int generateBuildScript(const QString &path) const
    {
        QFile scriptFile(path);
        // Warning: using QString maybe out of range
        QString command;
        QString templateName = "default.yaml";

        if (!scriptFile.open(QIODevice::WriteOnly)) {
            qCritical() << "failed to open" << path << scriptFile.error();
            return -1;
        }

        if (!BuilderConfig::instance()->getExec().isEmpty()) {
            command += BuilderConfig::instance()->getExec().toLocal8Bit() + "\n";
            scriptFile.write(command.toLocal8Bit());
            scriptFile.close();
            return 0;
        }
        // TODO: genarate global config, load from builder config file.
        command += "#global variable\n";
        command += QString("JOBS=%1\n").arg("6");
        command += QString("VERSION=%1\n").arg(q_ptr->package->version);

        if (q_ptr->config().targetArch() == "x86_64") {
            command += QString("ARCH=\"%1\"\n").arg("x86_64");
            command += QString("TRIPLET=\"%1\"\n").arg("x86_64-linux-gnu");
        } else if (q_ptr->config().targetArch() == "arm64") {
            command += QString("ARCH=\"%1\"\n").arg("arm64");
            command += QString("TRIPLET=\"%1\"\n").arg("aarch64-linux-gnu");
        } else if (q_ptr->config().targetArch() == "mips64el") {
            command += QString("ARCH=\"%1\"\n").arg("mips64el");
            command += QString("TRIPLET=\"%1\"\n").arg("mips64el-linux-gnuabi64");
        } else if (q_ptr->config().targetArch() == "sw_64") {
            command += QString("ARCH=\"%1\"\n").arg("sw_64");
            command += QString("TRIPLET=\"%1\"\n").arg("sw_64-linux-gnu");
        };

        // genarate local config, from *.yaml.
        if (q_ptr->build != nullptr) {
            if (q_ptr->build->kind == "manual") {
                templateName = "default.yaml";
            } else if (q_ptr->build->kind == "qmake") {
                templateName = "qmake.yaml";
            } else if (q_ptr->build->kind == "cmake") {
                templateName = "cmake.yaml";
            } else if (q_ptr->build->kind == "autotools") {
                templateName = "autotools.yaml";
            } else {
                qWarning().noquote() << QString("unknown build type: %1").arg(q_ptr->build->kind);
                return -1;
            }
        } else {
            qWarning().noquote() << "build rule not found, check your linglong.yaml";
            return -1;
        }

        auto templatePath =
            QStringList {BuilderConfig::instance()->templatePath(), templateName}.join(QDir::separator());

        QScopedPointer<Template> temp;
        YAML::Node tempNode;

        if (QFileInfo::exists(templatePath)) {
            tempNode = YAML::LoadFile(templatePath.toStdString());
        } else {
            QFile templateFile(QStringList {":", templateName}.join(QDir::separator()));

            if (templateFile.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream ts(&templateFile);
                QString fileContent = ts.readAll();

                tempNode = YAML::Load(fileContent.toStdString());
            }

            templateFile.close();
        }

        temp.reset(formYaml<Template>(tempNode));

        if (q_ptr->variables == nullptr) {
            auto variable = new Variables();
            q_ptr->variables = variable;
        }

        if (q_ptr->build->manual == nullptr) {
            auto buildManual = new BuildManual();
            q_ptr->build->manual = buildManual;
        }

        command += "#local variable\n";
        for (int i = q_ptr->variables->metaObject()->propertyOffset();
             i < q_ptr->variables->metaObject()->propertyCount(); ++i) {
            auto propertyName = q_ptr->variables->metaObject()->property(i).name();
            if (q_ptr->variables->property(propertyName).toString().isNull()) {
                command +=
                    QString("%1=\"%2\"\n").arg(propertyName).arg(temp->variables->property(propertyName).toString());
            } else {
                command +=
                    QString("%1=\"%2\"\n").arg(propertyName).arg(q_ptr->variables->property(propertyName).toString());
            }
        }

        command += "#build commands\n";
        command +=
            q_ptr->build->manual->configure.isNull() ? temp->build->manual->configure : q_ptr->build->manual->configure;
        command += q_ptr->build->manual->build.isNull() ? temp->build->manual->build : q_ptr->build->manual->build;
        command +=
            q_ptr->build->manual->install.isNull() ? temp->build->manual->install : q_ptr->build->manual->install;

        command += "\n";
        // strip script
        QFile stripScript(":/strip.sh");
        stripScript.open(QIODevice::ReadOnly);

        command += QString::fromLocal8Bit(stripScript.readAll());

        scriptFile.write(command.toLocal8Bit());
        stripScript.close();
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
}

void Project::generateBuildScript()
{
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

package::Ref Project::fullRef(const QString &channel, const QString &module) const
{
    return dd_ptr->fullRef(channel, module);
}

package::Ref Project::refWithModule(const QString &module) const
{
    return dd_ptr->refWithModule(module);
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
    return projectRoot;
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
    auto targetList = QStringList {rootPath(), ".linglong-target", project->package->id};
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
    if (PackageKindRuntime == project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    } else if (PackageKindLib == project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    } else if (PackageKindApp == project->package->kind) {
        return cacheAbsoluteFilePath({"runtime-install", subPath});
    };
    return QString();
}

QString Project::Config::cacheInstallPath(const QString &moduleDir, const QString &subPath) const
{
    if (PackageKindRuntime == project->package->kind) {
        return cacheAbsoluteFilePath({moduleDir, subPath});
    } else if (PackageKindLib == project->package->kind) {
        return cacheAbsoluteFilePath({moduleDir, subPath});
    } else if (PackageKindApp == project->package->kind) {
        return cacheAbsoluteFilePath({moduleDir, subPath});
    };
    return QString();
}

QString Project::Config::targetArch() const
{
    return util::hostArch();
}

QString Project::Config::targetInstallPath(const QString &sub) const
{
    if (PackageKindRuntime == project->package->kind) {
        return (sub.isEmpty() ? QString("/runtime") : QStringList {"/runtime", sub}.join("/"));
    } else if (PackageKindLib == project->package->kind) {
        return (sub.isEmpty() ? QString("/runtime") : QStringList {"/runtime", sub}.join("/"));
    } else if (PackageKindApp == project->package->kind) {
        return (sub.isEmpty() ? QString(QString("/opt/apps/%1/files").arg(project->ref().appId))
                              : QStringList {QString("/opt/apps/%1/files").arg(project->ref().appId), sub}.join("/"));
    };
    return QString();
}

} // namespace builder
} // namespace linglong
