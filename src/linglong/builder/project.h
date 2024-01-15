/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_PROJECT_H_
#define LINGLONG_SRC_BUILDER_BUILDER_PROJECT_H_

#include "linglong/package/ref.h"
#include "linglong/util/qserializer/deprecated.h"
#include "linglong/util/sysinfo.h"

#include <QObject>
#include <QScopedPointer>

namespace linglong {
namespace builder {

extern const char *const DependTypeRuntime;

extern const char *const BuildScriptPath;

extern const char *const PackageKindApp;
extern const char *const PackageKindLib;
extern const char *const PackageKindRuntime;

class Variables : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Variables)
public:
    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, triplet);
    Q_JSON_PROPERTY(QString, build_dir);
    Q_JSON_PROPERTY(QString, dest_dir);
    Q_JSON_PROPERTY(QString, conf_args);
    Q_JSON_PROPERTY(QString, extra_args);
    Q_JSON_PROPERTY(QString, jobs);
    Q_JSON_PROPERTY(QString, c_flags);
    Q_JSON_PROPERTY(QString, cxx_flags);
    Q_JSON_PROPERTY(QString, ld_flags);
};

class Environment : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Environment)
public:
    Q_JSON_PROPERTY(QString, CFLAGS);
    Q_JSON_PROPERTY(QString, CXXFLAGS);
    Q_JSON_PROPERTY(QString, LDFLAGS);
};

class Package : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Package)
public:
    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, kind);
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QString, description);
};

class BuilderRuntime : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(BuilderRuntime)
public:
    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, version);
    //    Q_JSON_PROPERTY(QString, locale);
};

class Source : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Source)
public:
    Q_JSON_PROPERTY(QString, kind);

    // diff with kind
    Q_JSON_PROPERTY(QString, url);
    //! the unique id of digest
    Q_JSON_PROPERTY(QString, digest);
    Q_JSON_PROPERTY(QString, version);

    // git
    Q_JSON_PROPERTY(QString, commit);

    Q_JSON_PROPERTY(QStringList, patch);
};

class BuildManual : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(BuildManual)
public:
    Q_JSON_PROPERTY(QString, configure);
    Q_JSON_PROPERTY(QString, build);
    Q_JSON_PROPERTY(QString, install);
};

class Build : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Build)
public:
    Q_JSON_PROPERTY(QString, kind);
    Q_JSON_PTR_PROPERTY(BuildManual, manual);
};

} // namespace builder
} // namespace linglong

namespace linglong {
namespace builder {

class BuildDepend : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(BuildDepend)
public:
    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QString, type);
};
} // namespace builder
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, BuildDepend)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Variables)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Package)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, BuilderRuntime)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Source)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Build)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, BuildManual)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Environment)

namespace linglong {
namespace builder {

class Project : public Serialize
{
    Q_OBJECT;

public:
    explicit Project(QObject *parent = nullptr);
    ~Project() override;

public:
    Q_JSON_PTR_PROPERTY(Variables, variables);
    Q_JSON_PTR_PROPERTY(Package, package);
    Q_JSON_PTR_PROPERTY(BuilderRuntime, runtime);
    Q_JSON_PTR_PROPERTY(BuilderRuntime, base);
    // WARNING: the default meta id is expanded form here, so keep it as full of namespace.
    Q_JSON_PROPERTY(QList<QSharedPointer<linglong::builder::BuildDepend>>, depends);
    Q_JSON_PTR_PROPERTY(Source, source);
    Q_JSON_PTR_PROPERTY(Build, build);
    Q_JSON_PTR_PROPERTY(Environment, environment);

public:
    package::Ref ref() const;
    package::Ref refWithModule(const QString &module) const;
    package::Ref runtimeRef() const;
    package::Ref baseRef() const;

    QString configFilePath; //! yaml path

    QString buildScriptPath() const { return this->scriptPath; }

private:
    QString scriptPath;

public:
    class Config
    {
    public:
        explicit Config(const QString &root, const Project *project)
            : projectRoot(root)
            , project(project)
        {
        }

        QString absoluteFilePath(const QStringList &filenames) const;

        // ${PROJECT_ROOT}/.linglong-target
        QString rootPath() const;
        QString cacheAbsoluteFilePath(const QStringList &filenames) const;
        QString cacheRuntimePath(const QString &subPath) const;
        QString cacheInstallPath(const QString &subPath) const;
        QString cacheInstallPath(const QString &moduleDir, const QString &subPath) const;

        QString targetArch() const;

        // in container, /runtime or /opt/apps/${appid}
        QString targetInstallPath(const QString &sub) const;

    private:
        const QString projectRoot;
        const Project *project;
    };

    const Config &config() const;

    void generateBuildScript();

private:
    Project::Config cfg;

    static QString buildArch() { return util::hostArch(); }

    int generateBuildScript(const QString &path);
};

package::Ref fuzzyRef(QSharedPointer<const JsonSerialize> obj);

} // namespace builder
} // namespace linglong

namespace linglong {
namespace builder {

// build template
class Template : public Serialize
{
    Q_OBJECT;
    Q_SERIALIZE_CONSTRUCTOR(Template)
public:
    Q_JSON_PTR_PROPERTY(Variables, variables);
    Q_JSON_PTR_PROPERTY(Build, build);
    Q_JSON_PTR_PROPERTY(Environment, environment);
};

QSERIALIZER_DECLARE(Template)
QSERIALIZER_DECLARE(Project)

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_PROJECT_H_
