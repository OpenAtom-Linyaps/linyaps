/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_RUNTIME_APP_H_
#define LINGLONG_SRC_MODULE_RUNTIME_APP_H_

#include "linglong/package/package.h"
#include "linglong/package/ref.h"
#include "linglong/runtime/app_config.h"
#include "linglong/runtime/container.h"
#include "linglong/runtime/oci.h"
#include "linglong/util/file.h"

namespace linglong::repo {
class Repo;
} // namespace linglong::repo

namespace linglong::runtime {

class Layer : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Layer)
    Q_JSON_PROPERTY(QString, ref);
};
} // namespace linglong::runtime

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, Layer)

namespace linglong::runtime {

class MountYaml : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(MountYaml)
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(QString, options);
    Q_JSON_PROPERTY(QString, source);
    Q_JSON_PROPERTY(QString, destination);
};
} // namespace linglong::runtime

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, MountYaml)

namespace linglong::runtime {
/*!
 * Permission: base for run, you can use full run or let it empty
 */
class AppPermission : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(AppPermission)
    Q_JSON_PROPERTY(QList<QSharedPointer<linglong::runtime::MountYaml>>, mounts);
};

} // namespace linglong::runtime

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, AppPermission)

namespace linglong::runtime {

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

    static auto load(linglong::repo::Repo *repo,
                     const linglong::package::Ref &ref,
                     const QString &desktopExec) -> QSharedPointer<App>;

    auto start() -> util::Error;

    void exec(QString cmd, QString env, QString cwd);

    void saveUserEnvList(const QStringList &userEnvList);

    void setAppParamMap(const ParamStringMap &paramMap);

    QSharedPointer<Container> container = nullptr;

private:
    auto init() -> bool;

    auto prepare() -> int;

    [[nodiscard]] auto stageSystem() const -> int;

    [[nodiscard]] auto stageRootfs(QString runtimeRootPath,
                                   const QString &appId,
                                   QString appRootPath) const -> int;

    [[nodiscard]] auto stageHost() const -> int;

    void stateDBusProxyArgs(bool enable, const QString &appId, const QString &proxyPath);

    // Fix to do 当前仅处理session bus
    auto stageDBusProxy(const QString &socketPath, bool useDBusProxy = false) -> int;

    [[nodiscard]] auto stageUser(const QString &appId) const -> int;

    auto stageMount() -> int;

    auto mountTmp() -> int;

    auto fixMount(QString runtimeRootPath, const QString &appId) -> int;

    static auto getMathedRuntime(const QString &runtimeId, const QString &runtimeVersion)
      -> QString;

    // FIXME: none static
    static auto loadConfig(linglong::repo::Repo *repo,
                           const QString &appId,
                           const QString &appVersion,
                           const QString &channel,
                           const QString &module) -> QString;

    QString desktopExec = nullptr;
    ParamStringMap envMap;
    ParamStringMap runParamMap;

    QSharedPointer<Runtime> r = nullptr;
    QSharedPointer<AppConfig> appConfig = nullptr;

    repo::Repo *repo = nullptr;
    int sockets[2]; // save file describers of sockets used to communicate with ll-box

    const QString sysLinglongInstalltions = util::getLinglongRootPath() + "/entries/share";
};

namespace PrivateAppInit {
auto init() -> int;
static int _ = init();
} // namespace PrivateAppInit

} // namespace linglong::runtime

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::runtime, App)
#endif
