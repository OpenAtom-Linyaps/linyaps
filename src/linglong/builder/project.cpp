/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "project.h"

#include "builder_config.h"
#include "linglong/package/ref.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/sysinfo.h"

#include <QDebug>

namespace linglong {
namespace builder {

QSERIALIZER_IMPL(Variables);
QSERIALIZER_IMPL(Environment);
QSERIALIZER_IMPL(Package);
QSERIALIZER_IMPL(BuilderRuntime);
QSERIALIZER_IMPL(Source);
QSERIALIZER_IMPL(BuildManual);
QSERIALIZER_IMPL(Build);
QSERIALIZER_IMPL(BuildDepend);
QSERIALIZER_IMPL(Project);
QSERIALIZER_IMPL(Template);

const char *const DependTypeRuntime = "runtime";

const char *const BuildScriptPath = "/entry.sh";
// const char *const BuildCacheDirectoryName = "linglong-builder";

const char *const PackageKindApp = "app";
const char *const PackageKindLib = "lib";
const char *const PackageKindRuntime = "runtime";

package::Ref Project::ref() const
{
    return package::Ref("", package->id, package->version, buildArch());
}

void Project::generateBuildScript()
{
    // TODO: how to define an build task
    auto cacheDirectory = config().cacheAbsoluteFilePath({ "tmp" });
    util::ensureDir(cacheDirectory);
    auto scriptPath = cacheDirectory + BuildScriptPath;
    generateBuildScript(scriptPath);
    this->scriptPath = scriptPath;

    // get source.version form package.version
    if (source && source->version.isEmpty()) {
        source->version = package->version;
    }
}

int Project::generateBuildScript(const QString &path)
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
        auto exec = BuilderConfig::instance()->getExec();
        for (const auto &arg : exec) {
            command += '\'' + arg + '\'' + ' ';
        }
        command = command.trimmed();
        command += "\n";
        scriptFile.write(command.toLocal8Bit());
        scriptFile.close();
        return 0;
    }
    // TODO: generate global config, load from builder config file.
    command += "#global variable\n";
    command += QString("JOBS=%1\n").arg("6");
    command += QString("VERSION=%1\n").arg(package->version);
    command += QString("APPID=%1\n").arg(package->id);

    if (config().targetArch() == "x86_64") {
        command += QString("ARCH=\"%1\"\n").arg("x86_64");
        command += QString("TRIPLET=\"%1\"\n").arg("x86_64-linux-gnu");
    } else if (config().targetArch() == "arm64") {
        command += QString("ARCH=\"%1\"\n").arg("arm64");
        command += QString("TRIPLET=\"%1\"\n").arg("aarch64-linux-gnu");
    } else if (config().targetArch() == "mips64el") {
        command += QString("ARCH=\"%1\"\n").arg("mips64el");
        command += QString("TRIPLET=\"%1\"\n").arg("mips64el-linux-gnuabi64");
    } else if (config().targetArch() == "sw_64") {
        command += QString("ARCH=\"%1\"\n").arg("sw_64");
        command += QString("TRIPLET=\"%1\"\n").arg("sw_64-linux-gnu");
    };

    // generate local config, from *.yaml.
    if (build != nullptr) {
        if (build->kind == "manual") {
            templateName = "default.yaml";
        } else if (build->kind == "qmake") {
            templateName = "qmake.yaml";
        } else if (build->kind == "cmake") {
            templateName = "cmake.yaml";
        } else if (build->kind == "autotools") {
            templateName = "autotools.yaml";
        } else if (build->kind == "makeimage") {
            templateName = "makeimage.yaml";
        } else {
            qWarning().noquote() << QString("unknown build type: %1").arg(build->kind);
            return -1;
        }
    } else {
        qWarning().noquote() << "build rule not found, check your linglong.yaml";
        return -1;
    }

    auto templatePath = QStringList{ BuilderConfig::instance()->templatePath(), templateName }.join(
      QDir::separator());

    util::Error err;
    QSharedPointer<Template> temp;
    if (QFileInfo::exists(templatePath)) {
        std::tie(temp, err) = util::fromYAML<QSharedPointer<Template>>(templatePath);
    } else {
        QFile templateFile(QStringList{ ":", templateName }.join(QDir::separator()));
        if (templateFile.open(QFile::ReadOnly | QFile::Text)) {
            std::tie(temp, err) = util::fromYAML<QSharedPointer<Template>>(templateFile.readAll());
        }
    }

    if (variables == nullptr) {
        variables.reset(new Variables());
    }

    if (environment == nullptr) {
        environment.reset(new Environment());
    }

    if (build->manual == nullptr) {
        build->manual.reset(new BuildManual());
    }

    Q_ASSERT(temp->variables != nullptr);
    Q_ASSERT(temp->environment != nullptr);

    command += "#local variable\n";
    for (int i = variables->metaObject()->propertyOffset();
         i < variables->metaObject()->propertyCount();
         ++i) {
        auto propertyName = variables->metaObject()->property(i).name();
        if (variables->property(propertyName).toString().isNull()) {
            command += QString("%1=\"%2\"\n")
                         .arg(propertyName)
                         .arg(temp->variables->property(propertyName).toString());
        } else {
            command += QString("%1=\"%2\"\n")
                         .arg(propertyName)
                         .arg(variables->property(propertyName).toString());
        }
    }
    // set build environment variables
    command += "#environment variables\n";
    for (int i = environment->metaObject()->propertyOffset();
         i < environment->metaObject()->propertyCount();
         ++i) {
        auto propertyName = environment->metaObject()->property(i).name();
        if (environment->property(propertyName).toString().isNull()) {
            command += QString("export %1=\"%2\"\n")
                         .arg(propertyName)
                         .arg(temp->environment->property(propertyName).toString());
        } else {
            command += QString("export %1=\"%2\"\n")
                         .arg(propertyName)
                         .arg(environment->property(propertyName).toString());
        }
    }

    command += "#build commands\n";
    command +=
      build->manual->configure.isNull() ? temp->build->manual->configure : build->manual->configure;
    command += build->manual->build.isNull() ? temp->build->manual->build : build->manual->build;
    command +=
      build->manual->install.isNull() ? temp->build->manual->install : build->manual->install;

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

Project::Project(QObject *parent)
    : JsonSerialize(parent)
    , cfg(BuilderConfig::instance()->getProjectRoot(), this)
{
}

package::Ref Project::refWithModule(const QString &module) const
{
    return package::Ref("", "main", package->id, package->version, buildArch(), module);
}

const Project::Config &Project::config() const
{
    return cfg;
}

package::Ref Project::runtimeRef() const
{
    return fuzzyRef(runtime);
}

package::Ref Project::baseRef() const
{
    return fuzzyRef(base);
}

Project::~Project() = default;

QString Project::Config::rootPath() const
{
    return projectRoot;
}

QString Project::Config::absoluteFilePath(const QStringList &filenames) const
{
    auto targetList = QStringList{ rootPath() };
    targetList.append(filenames);
    auto target = targetList.join("/");

    return target;
}

QString Project::Config::cacheAbsoluteFilePath(const QStringList &filenames) const
{
    auto targetList = QStringList{ rootPath(), ".linglong-target", project->package->id };
    targetList.append(filenames);
    auto target = targetList.join("/");
    return target;
}

QString Project::Config::cacheRuntimePath(const QString &subPath) const
{
    return cacheAbsoluteFilePath({ "runtime", subPath });
}

QString Project::Config::cacheInstallPath(const QString &subPath) const
{
    if (PackageKindRuntime == project->package->kind) {
        return cacheAbsoluteFilePath({ "runtime-install", subPath });
    } else if (PackageKindLib == project->package->kind) {
        return cacheAbsoluteFilePath({ "runtime-install", subPath });
    } else if (PackageKindApp == project->package->kind) {
        return cacheAbsoluteFilePath({ "runtime-install", subPath });
    };
    return QString();
}

QString Project::Config::cacheInstallPath(const QString &moduleDir, const QString &subPath) const
{
    if (PackageKindRuntime == project->package->kind) {
        return cacheAbsoluteFilePath({ moduleDir, subPath });
    } else if (PackageKindLib == project->package->kind) {
        return cacheAbsoluteFilePath({ moduleDir, subPath });
    } else if (PackageKindApp == project->package->kind) {
        return cacheAbsoluteFilePath({ moduleDir, subPath });
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
        return (sub.isEmpty() ? QString("/runtime") : QStringList{ "/runtime", sub }.join("/"));
    } else if (PackageKindLib == project->package->kind) {
        return (sub.isEmpty() ? QString("/runtime") : QStringList{ "/runtime", sub }.join("/"));
    } else if (PackageKindApp == project->package->kind) {
        return (
          sub.isEmpty()
            ? QString(QString("/opt/apps/%1/files").arg(project->ref().appId))
            : QStringList{ QString("/opt/apps/%1/files").arg(project->ref().appId), sub }.join(
              "/"));
    };
    return QString();
}

} // namespace builder
} // namespace linglong
