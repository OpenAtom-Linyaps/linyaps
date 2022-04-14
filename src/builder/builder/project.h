/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QScopedPointer>

#include "module/util/json.h"
#include "module/package/ref.h"

namespace linglong {
namespace builder {

extern const char *DependTypeRuntime;

extern const char *BuildScriptPath;

extern const char *PackageKindApp;
extern const char *PackageKindLib;
extern const char *PackageKindRuntime;

class Variables : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Variables)
public:
    Q_JSON_PROPERTY(QString, id);
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

namespace linglong {
namespace builder {

class ProjectPrivate;
class Project : public JsonSerialize
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
    Q_JSON_PROPERTY(linglong::builder::BuildDependList, depends);
    Q_JSON_PTR_PROPERTY(Source, source);
    Q_JSON_PTR_PROPERTY(Build, build);

public:
    package::Ref ref() const;
    package::Ref runtimeRef() const;
    package::Ref baseRef() const;

    QString configFilePath() const;
    void setConfigFilePath(const QString &configFilePath);

    QString buildScriptPath() const;

public:
    class Config
    {
    public:
        explicit Config(const QString &root, const Project *project)
            : m_projectRoot(root)
            , m_project(project)
        {
        }

        QString absoluteFilePath(const QStringList &filenames) const;

        // ${PROJECT_ROOT}/.linglong-target
        QString rootPath() const;
        QString cacheAbsoluteFilePath(const QStringList &filenames) const;
        QString cacheRuntimePath(const QString &subPath) const;
        QString cacheInstallPath(const QString &subPath) const;

        QString targetArch() const;

        // in container, /runtime or /opt/apps/${appid}
        QString targetInstallPath(const QString &sub) const;

    private:
        const QString m_projectRoot;
        const Project *m_project;
    };

    const Config &config() const;

    void onPostSerialize() override;

private:
    QScopedPointer<ProjectPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Project)
};

package::Ref fuzzyRef(const JsonSerialize *obj);

} // namespace builder
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::builder, Project)
