/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "app.h"

#include <QProcess>
#include <QFile>
#include <QStandardPaths>

#include <unistd.h>
#include <sys/wait.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <QDir>

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/json.h"
#include "module/util/fs.h"

#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

using namespace linglong;

namespace {

// const char *DefaultRuntimeID = "com.deepin.Runtime";

//! create an uuid dir, and run container
//! TODO: use file lock to make sure only one process create
//! \return
QString ensureContainerPath()
{
    auto containerID = linglong::util::genUUID();
    auto runtimePath = QStandardPaths::standardLocations(QStandardPaths::RuntimeLocation).value(0);
    QDir runtimeDir(runtimePath);
    auto path = runtimeDir.absoluteFilePath(QStringList {"linglong", containerID}.join("/"));
    runtimeDir.mkpath(path);
    return path;
}

} // namespace

class AppPrivate
{
public:
    explicit AppPrivate(App *parent)
        : q_ptr(parent)
    {
    }

    bool init()
    {
        QFile jsonFile(":/config.json");
        jsonFile.open(QIODevice::ReadOnly);
        auto json = QJsonDocument::fromJson(jsonFile.readAll());
        r = fromVariant<Runtime>(json.toVariant());
        r->setParent(q_ptr);
        return true;
    }

    int prepare()
    {
        Q_Q(App);
        stageSystem();
        // FIXME: get info from module/package
        //        auto runtimeID = q->runtime->id;
        QString runtimeRootPath = q->runtime->rootPath;
        qDebug() << "prepare runtime rootPath" << q->runtime->rootPath;
        stageRuntime(runtimeRootPath);

        QString appID = q->package->id;
        QString appRootPath = q->package->rootPath;
        stageApp(appID, appRootPath);

        stageHost();
        stageUser(appID);

        stageMount();

        // FIXME: read from desktop file
        if (r->process->args.isEmpty()) {
            r->process->args = q->package->args;
        }
        qDebug() << "exec" << r->process->args;
        return 0;
    }

    int stageSystem() const
    {
        Mount &m = *new Mount(r);
        m.type = "bind";
        m.options = QStringList {"bind"};
        m.type = "bind";

        m.destination = "/dev/dri";
        m.source = "/dev/dri";
        r->mounts.push_front(&m);

        return 0;
    }

    int stageRuntime(const QString &runtimeRootPath) const
    {
        Mount &m = *new Mount(r);
        m.type = "bind";
        m.options = QStringList {"ro"};
        m.type = "bind";

        m.destination = "/usr";
        m.source = runtimeRootPath;
        // FIXME: if runtime is empty, use the last
        if (m.source.isEmpty()) {
            qCritical() << "mount runtime failed" << runtimeRootPath;
            return -1;
        }
        qDebug() << "mount runtime" << m.source << m.destination;
        r->mounts.push_front(&m);

        return 0;
    }

    int stageApp(const QString &appID, const QString &appRootPath) const
    {
        {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rw"};
            m.type = "bind";

            m.destination = "/opt/apps/" + appID;
            m.source = appRootPath;
            r->mounts.push_back(&m);
        }

        // FIXME: only app.conf
        //    m.destination = "/run/layer/ld.so.conf.d/";
        //    m.source = util::jonsPath({appInfo.rootPath(), "entries", "ld.so.conf.d"});
        //    r.mounts.push_front(m);

        // FIXME: not work because readonly etc
        {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rw"};
            m.type = "bind";
            m.destination = "/run/app/libs";
            m.source = QStringList {appRootPath, "files/libs"}.join("/");
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        //        for (auto const &f : appInfo.suidFiles()) {
        //            Hook &h = *new Hook(r);
        //            h.path = "/usr/bin/chown";
        //            h.args = std::vector<std::string> {
        //                "0",
        //                linglong::util::jonsPath({"/opt/apps", appInfo.appID(), f.toStdString()}),
        //            };
        //            r->hooks->poststart.push_back(&h);
        //
        //            h.path = "/usr/bin/chmod";
        //            h.args = std::vector<std::string> {
        //                "u+s",
        //                util::jonsPath({"/opt/apps", appInfo.appID(), f.toStdString()}),
        //            };
        //            r->hooks->poststart.push_back(&h);
        //        }

        // FIXME: let application do this, ld config
        auto ldLibraryPath = QStringList {"/opt/apps", appID, "files/libs"}.join("/");
        r->process->env.push_back("LD_LIBRARY_PATH=" + ldLibraryPath);
        r->process->env.push_back("QT_PLUGIN_PATH=/usr/lib/plugins");
        r->process->env.push_back("QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins/platforms");
        return 0;
    }

    int stageHost() const
    {
        QList<QPair<QString, QString>> mountMap = {
            {"/etc/resolv.conf", "/run/host/network/etc/resolv.conf"},
            {"/run/resolvconf", "/run/resolvconf"},
            {"/tmp/.X11-unix", "/tmp/.X11-unix"},
            {"/usr/share/fonts", "/run/host/appearance/fonts"},
            {"/var/cache/fontconfig", "/run/host/appearance/fonts-cache"},
            {"/usr/share/locale/", "/usr/share/locale/"},
            {"/usr/lib/locale/", "/usr/lib/locale/"},
            {"/usr/share/fonts", "/usr/share/fonts"},
            {"/usr/share/themes", "/usr/share/themes"},
            {"/usr/share/icons", "/usr/share/icons"},
            {"/usr/share/zoneinfo", "/usr/share/zoneinfo"},
            {"/etc/localtime", "/run/host/etc/localtime"},
            {"/etc/machine-id", "/run/host/etc/machine-id"},
            {"/var", "/var"}};

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"ro"};
            m.type = "bind";
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        return 0;
    }

    int stageUser(const QString &appID) const
    {
        QList<QPair<QString, QString>> mountMap;

        // bind user data
        auto userRuntimeDir = QString("/run/user/%1").arg(getuid());
        auto hostRuntimeDir = util::ensureUserDir({".linglong", appID, "runtime"});

        mountMap.push_back(qMakePair(hostRuntimeDir, userRuntimeDir));

        // FIXME: use proxy dbus
        bool useDBusProxy = true;
        if (useDBusProxy) {
            // bind dbus-proxy-user, now use session bus
            mountMap.push_back(qMakePair(
                userRuntimeDir + "/user-bus",
                userRuntimeDir + "/bus"));
            // bind dbus-proxy
            mountMap.push_back(qMakePair(
                userRuntimeDir + "/system-bus",
                QString("/run/dbus/system_bus_socket")));
        } else {
            mountMap.push_back(qMakePair(
                userRuntimeDir + "/bus",
                userRuntimeDir + "/bus"));
            mountMap.push_back(qMakePair(
                QString("/run/dbus/system_bus_socket"),
                QString("/run/dbus/system_bus_socket")));
        }

        auto hosApptHome = util::ensureUserDir({".linglong", appID, "home"});
        mountMap.push_back(qMakePair(hosApptHome, util::getUserFile("")));

        auto appConfigPath = util::ensureUserDir({".linglong", appID, "/config"});
        mountMap.push_back(qMakePair(appConfigPath, util::getUserFile(".config")));

        auto appCachePath = util::ensureUserDir({".linglong", appID, "/cache"});
        mountMap.push_back(qMakePair(appCachePath, util::getUserFile(".cache")));

        mountMap.push_back(qMakePair(userRuntimeDir + "/dconf",
                                     userRuntimeDir + "/dconf"));

        mountMap.push_back(qMakePair(
            util::getUserFile(".config/user-dirs.dirs"),
            util::getUserFile(".config/user-dirs.dirs")));

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {};

            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        QList<QPair<QString, QString>> roMountMap;
        roMountMap.push_back(qMakePair(
            util::getUserFile(".local/share/fonts"),
            util::getUserFile(".local/share/fonts")));

        roMountMap.push_back(qMakePair(
            util::getUserFile(".config/fontconfig"),
            util::getUserFile(".config/fontconfig")));

        // mount fonts
        roMountMap.push_back(qMakePair(
            util::getUserFile(".local/share/fonts"),
            QString("/run/host/appearance/user-fonts")));

        // mount fonts cache
        mountMap.push_back(qMakePair(
            util::getUserFile(".cache/fontconfig"),
            QString("/run/host/appearance/user-fonts-cache")));

        QString xauthority = getenv("XAUTHORITY");
        roMountMap.push_back(qMakePair(xauthority, xauthority));

        for (const auto &pair : roMountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"ro"};

            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        r->process->env.push_back("HOME=" + util::getUserFile(""));
        r->process->env.push_back("XDG_RUNTIME_DIR=" + userRuntimeDir);
        r->process->env.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path=" + util::jonsPath({userRuntimeDir, "bus"}));

        auto bypassENV = [&](const char *constEnv) {
            r->process->env.push_back(QString(constEnv) + "=" + getenv(constEnv));
        };

        QStringList envList = {
            "DISPLAY",
            "LANG",
            "LANGUAGE",
            "XAUTHORITY",
            "XDG_SESSION_DESKTOP",
            "D_DISABLE_RT_SCREEN_SCALE",
            "XMODIFIERS",
            "DESKTOP_SESSION",
            "DEEPIN_WINE_SCALE",
            "XDG_CURRENT_DESKTOP",
            "XIM",
            "XDG_SESSION_TYPE=x11",
            "CLUTTER_IM_MODULE",
            "QT4_IM_MODULE",
            "GTK_IM_MODULE"};

        for (auto &env : envList) {
            bypassENV(env.toStdString().c_str());
        }
        qCritical() << r->process->env;
        r->process->cwd = util::getUserFile("");

        QList<QList<quint64>> uidMaps = {
            // FIXME: get 65534 form nobody
            {65534, 0, 1},
            {getuid(), getuid(), 1},
        };
        for (auto const &uidMap : uidMaps) {
            Q_ASSERT(uidMap.size() == 3);
            auto idMap = new IDMap(r->linux);
            idMap->hostID = uidMap.value(0);
            idMap->containerID = uidMap.value(1);
            idMap->size = uidMap.value(2);
            r->linux->uidMappings.push_back(idMap);
        }

        QList<QList<quint64>> gidMaps = {
            // FIXME: get 65534 form nogroup
            {65534, 0, 1},
            {getgid(), getgid(), 1},
        };
        for (auto const &gidMap : gidMaps) {
            Q_ASSERT(gidMap.size() == 3);
            auto idMap = new IDMap(r->linux);
            idMap->hostID = gidMap.value(0);
            idMap->containerID = gidMap.value(1);
            idMap->size = gidMap.value(2);
            r->linux->gidMappings.push_back(idMap);
        }

        return 0;
    }

    int stageMount()
    {
        Q_Q(const App);

        QMap<QString, std::function<QString()>> replacementMap = {
            {"${HOME}", []() -> QString {
                 return util::getUserFile("");
             }},
        };

        auto pathPreprocess = [&](QString path) -> QString {
            auto keys = replacementMap.keys();
            for (const auto &key : keys) {
                path.replace(key, (replacementMap.value(key))());
            }
            return path;
        };

        for (const auto &mount : q->mounts) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {};
            auto component = mount.split(":");
            m.source = pathPreprocess(component.value(0));
            m.destination = pathPreprocess(component.value(1));
            r->mounts.push_back(&m);
            qCritical() << "mount app" << m.source << m.destination;
        }

        return 0;
    }

    Runtime *r = nullptr;
    App *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(App);
};

App::App(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new AppPrivate(this))
{
}

App *App::load(const QString &configFilepath)
{
    // TODO: fixme, replace with real conf
    QFile appConfig(":/app-test.yaml");
    appConfig.open(QIODevice::ReadOnly);
    App *app = nullptr;
    try {
        YAML::Node doc = YAML::Load(appConfig.readAll().toStdString());
        app = formYaml<App>(doc);
    } catch (...) {
        qCritical() << "FIXME: load config failed, use default app config";
    }
    app->dd_ptr->init();
    return app;
}

int App::start()
{
    Q_D(App);
    QString containerRoot = ensureContainerPath();
    d->r->root->path = containerRoot;

    d->prepare();

    // write pid file
    QFile pidFile(containerRoot + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << containerRoot;
    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(d->r)).toJson();
    auto data = json.toStdString();

    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) {
        return EXIT_FAILURE;
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    pid_t boxPid = fork();
    if (0 == boxPid) {
        // child process
        (void)close(pipeEnds[1]);
        if (dup2(pipeEnds[0], LINGLONG) == -1) {
            return EXIT_FAILURE;
        }
        (void)close(pipeEnds[0]);
        char const *const args[] = {"/usr/bin/ll-box", LL_TOSTRING(LINGLONG), NULL};
        execvp(args[0], (char **)args);
    } else {
        close(pipeEnds[0]);
        write(pipeEnds[1], data.c_str(), data.size());
        close(pipeEnds[1]);

        // FIXME(interactive bash): if need keep interactive shell
        waitpid(boxPid, nullptr, 0);
    }

    return EXIT_SUCCESS;
}

App::~App() = default;
