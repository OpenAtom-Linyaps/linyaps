/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_APP_H_
#define LINGLONG_SRC_MODULE_RUNTIME_APP_H_

#include "container.h"
#include "module/package/package.h"
#include "module/package/ref.h"

namespace linglong {
namespace repo {
class Repo;
}
} // namespace linglong

namespace linglong {
namespace runtime {

class Layer : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Layer)
    Q_JSON_PROPERTY(QString, ref);
};
} // namespace runtime
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, Layer)

namespace linglong {
namespace runtime {

class MountYaml : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(MountYaml)
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, options);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QString, destination);
};
} // namespace runtime
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, MountYaml)

namespace linglong {
namespace runtime {
/*!
 * Permission: base for run, you can use full run or let it empty
 */
class AppPermission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(AppPermission)
    Q_JSON_PROPERTY(QList<QSharedPointer<linglong::runtime::MountYaml>>, mounts);
};

} // namespace runtime
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, AppPermission)

namespace linglong {
namespace runtime {

class AppPrivate;

class App : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PTR_PROPERTY(Layer, package);
    Q_JSON_PTR_PROPERTY(Layer, runtime);

    // TODO: should config base mount point
    Q_JSON_PTR_PROPERTY(linglong::runtime::AppPermission, permissions);

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    static QSharedPointer<App> load(linglong::repo::Repo *repo,
                                    const linglong::package::Ref &ref,
                                    const QString &desktopExec);

    QSharedPointer<const Container> container() const;

    int start();
    void exec(QString cmd, QString env, QString cwd);

    void saveUserEnvList(const QStringList &userEnvList);

    void setAppParamMap(const ParamStringMap &paramMap);

private:
    QScopedPointer<AppPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), App)
};

namespace PrivateAppInit {
int init();
static int _ = init();
} // namespace PrivateAppInit

} // namespace runtime
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, App)
#endif
