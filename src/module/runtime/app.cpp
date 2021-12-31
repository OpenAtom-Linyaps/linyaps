/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"

#include <unistd.h>
#include <sys/wait.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <QProcess>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/json.h"
#include "module/util/fs.h"
#include "module/util/xdg.h"
#include "module/util/desktop_entry.h"
#include "module/package/info.h"
#include "module/repo/repo.h"
#include "module/flatpak/flatpak_manager.h"

#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

using namespace linglong;

namespace linglong {
namespace util {
// TODO: move to util
bool ensureDir(const QString &path)
{
    QDir dir(path);
    dir.mkpath(".");
    return true;
}
} // namespace util
} // namespace linglong

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

        container = new Container(q_ptr);
        container->ID = linglong::util::genUUID();
        container->WorkingDirectory =
            util::userRuntimeDir().absoluteFilePath(QString("linglong/%1").arg(container->ID));
        util::ensureDir(container->WorkingDirectory);
        return true;
    }

    int prepare()
    {
        Q_Q(App);

        // FIXME: get info from module/package
        auto runtimeRef = package::Ref(q->runtime->ref);
        QString runtimeRootPath = repo::rootOfLayer(runtimeRef);

        // FIXME: return error if files not exist
        auto fixRuntimePath = runtimeRootPath + "/files";
        if (!util::dirExists(fixRuntimePath)) {
            fixRuntimePath = runtimeRootPath;
        }

        auto appRef = package::Ref(q->package->ref);
        QString appRootPath = repo::rootOfLayer(appRef);

        stageRootfs(runtimeRef.id, fixRuntimePath, appRef.id, appRootPath);

        stageSystem();
        stageHost();
        stageUser(appRef.id);
        stageMount();

        auto envFilepath = container->WorkingDirectory + QString("/env");
        QFile envFile(envFilepath);
        if (!envFile.open(QIODevice::WriteOnly)) {
            qCritical() << "create env failed" << envFile.error();
        }
        for (const auto &env : r->process->env) {
            envFile.write(env.toLocal8Bit());
            envFile.write("\n");
        }
        envFile.close();

        Mount &m = *new Mount(r);
        m.type = "bind";
        m.options = QStringList {};
        m.source = envFilepath;
        m.destination = "/run/app/env";
        r->mounts.push_back(&m);

        // TODO: move to class package
        // find desktop file
        QDir applicationsDir(QStringList {appRootPath, "entries", "applications"}.join(QDir::separator()));
        auto desktopFilenameList = applicationsDir.entryList({"*.desktop"}, QDir::Files);
        if (useFlatpakRuntime) {
            desktopFilenameList = flatpak::FlatpakManager::instance()->getAppDesktopFileList(appRef.id);
        }
        if (desktopFilenameList.length() <= 0) {
            return -1;
        }

        util::DesktopEntry desktopEntry(applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));

        if (r->process->args.isEmpty()) {
            r->process->args = util::parseExec(desktopEntry.rawValue("Exec"));
        }

        qDebug() << "exec" << r->process->args;
        return 0;
    }

    int stageSystem() const
    {
        QList<QPair<QString, QString>> mountMap;
        mountMap = {
            {"/dev/dri", "/dev/dri"},
            {"/dev/snd", "/dev/snd"},
        };

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"bind"};
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount stageSystem" << m.source << m.destination;
        }
        return 0;
    }

    int stageRootfs(const QString &runtimeID, QString runtimeRootPath, const QString &appID, QString appRootPath) const
    {
        bool useThinRuntime = true;
        bool fuseMount = false;

        // if use wine runtime, mount with fuse
        // FIXME(iceyer): use info.json to decide use fuse or not
        if (runtimeRootPath.contains("org.deepin.Wine")) {
            fuseMount = true;
        }

        if (useFlatpakRuntime) {
            fuseMount = false;
            useThinRuntime = false;
        }

        r->annotations = new Annotations(r);
        r->annotations->container_root_path = container->WorkingDirectory;

        if (fuseMount) {
            r->annotations->overlayfs = new AnnotationsOverlayfsRootfs(r->annotations);
            r->annotations->overlayfs->lower_parent =
                QStringList {container->WorkingDirectory, ".overlayfs", "lower_parent"}.join("/");
            r->annotations->overlayfs->upper =
                QStringList {container->WorkingDirectory, ".overlayfs", "upper"}.join("/");
            r->annotations->overlayfs->workdir =
                QStringList {container->WorkingDirectory, ".overlayfs", "workdir"}.join("/");
        } else {
            r->annotations->native = new AnnotationsNativeRootfs(r->annotations);
        }

        QList<QPair<QString, QString>> mountMap;

        if (useThinRuntime) {
            mountMap = {
                {"/usr", "/usr"},
                {"/etc", "/etc"},
                {runtimeRootPath + "/usr", "/runtime"},
            };

            // FIXME(iceyer): extract for wine, remove later
            if (fuseMount) {
                // NOTE: the override should be before host /usr, MUST in the front of all
                mountMap.push_front({runtimeRootPath + "/usr", "/usr"});
                mountMap.push_back({runtimeRootPath + "/opt/deepinwine", "/opt/deepinwine"});
                mountMap.push_back({runtimeRootPath + "/opt/deepin-wine6-stable", "/opt/deepin-wine6-stable"});
            }
        } else {
            if (useFlatpakRuntime) {
                runtimeRootPath = flatpak::FlatpakManager::instance()->getRuntimePath(appID);
            }
            // FIXME(iceyer): if runtime is empty, use the last
            if (runtimeRootPath.isEmpty()) {
                qCritical() << "mount runtime failed" << runtimeRootPath;
                return -1;
            }

            mountMap.push_back({runtimeRootPath, "/usr"});
        }

        if (useFlatpakRuntime) {
            appRootPath = flatpak::FlatpakManager::instance()->getAppPath(appID);
            mountMap.push_back({appRootPath, "/app"});
        } else {
            mountMap.push_back({appRootPath, "/opt/apps/" + appID});
            // TODO(iceyer): add doc for this or remove
            mountMap.push_back({QStringList {appRootPath, "files/lib"}.join("/"), "/run/app/lib"});
        }

        for (const auto &pair : mountMap) {
            auto m = new Mount(r);
            m->type = "bind";
            m->options = QStringList {"ro"};
            m->source = pair.first;
            m->destination = pair.second;

            if (fuseMount) {
                r->annotations->overlayfs->mounts.push_back(m);
            } else {
                r->annotations->native->mounts.push_back(m);
            }
        }

        // TODO(iceyer): let application do this or add to doc
        auto appLdLibraryPath = QStringList {"/opt/apps", appID, "files/lib"}.join("/");
        if (useFlatpakRuntime) {
            appLdLibraryPath = "/app/lib";
            r->process->env.push_back(
                "XDG_DATA_DIRS=/app/share:/usr/share:/usr/share/runtime/share:/run/host/user-share:/run/host/share");
        }

        // TODO(iceyer): support other arch, or just no arch?
        auto fixLdLibraryPath = QStringList {
            appLdLibraryPath,
            "/runtime/lib",
            "/runtime/lib/x86_64-linux-gnu",
            "/runtime/lib/i386-linux-gnu",
        };
        r->process->env.push_back("LD_LIBRARY_PATH=" + fixLdLibraryPath.join(":"));
        r->process->env.push_back("QT_PLUGIN_PATH=/usr/lib/plugins:/runtime/plugins");
        r->process->env.push_back("QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins/platforms:/runtime/plugins/platforms");
        return 0;
    }

    int stageHost() const
    {
        QList<QPair<QString, QString>> mountMap = {
            {"/tmp/.X11-unix", "/tmp/.X11-unix"},
            {"/etc/resolv.conf", "/run/host/network/etc/resolv.conf"},
            {"/run/resolvconf", "/run/resolvconf"},
            {"/usr/share/fonts", "/run/host/appearance/fonts"},
            {"/usr/share/locale/", "/usr/share/locale/"},
            {"/usr/lib/locale/", "/usr/lib/locale/"},
            {"/usr/share/fonts", "/usr/share/fonts"},
            {"/usr/share/themes", "/usr/share/themes"},
            {"/usr/share/icons", "/usr/share/icons"},
            {"/usr/share/zoneinfo", "/usr/share/zoneinfo"},
            {"/etc/localtime", "/run/host/etc/localtime"},
            {"/etc/machine-id", "/run/host/etc/machine-id"},
            {"/etc/machine-id", "/etc/machine-id"},
            {"/var", "/var"},
            {"/var/cache/fontconfig", "/run/host/appearance/fonts-cache"},
        };

        for (auto const &item : QDir("/dev").entryInfoList({"nvidia*"}, QDir::AllEntries | QDir::System)) {
            mountMap.push_back({item.canonicalFilePath(), item.canonicalFilePath()});
        }

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"ro"};
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
        {
            Mount &m = *new Mount(r);
            m.type = "tmpfs";
            m.options = QStringList {"nodev", "nosuid"};
            m.source = "tmpfs";
            m.destination = userRuntimeDir;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        // FIXME: use proxy dbus
        bool useDBusProxy = false;
        if (useDBusProxy) {
            // bind dbus-proxy-user, now use session bus
            mountMap.push_back(qMakePair(userRuntimeDir + "/user-bus", userRuntimeDir + "/bus"));
            // bind dbus-proxy
            mountMap.push_back(qMakePair(userRuntimeDir + "/system-bus", QString("/run/dbus/system_bus_socket")));
        } else {
            mountMap.push_back(qMakePair(userRuntimeDir + "/bus", userRuntimeDir + "/bus"));
            mountMap.push_back(
                qMakePair(QString("/run/dbus/system_bus_socket"), QString("/run/dbus/system_bus_socket")));
        }

        auto hostAppHome = util::ensureUserDir({".linglong", appID, "home"});
        mountMap.push_back(qMakePair(hostAppHome, util::getUserFile("")));

        auto appConfigPath = util::ensureUserDir({".linglong", appID, "/config"});
        mountMap.push_back(qMakePair(appConfigPath, util::getUserFile(".config")));

        auto appCachePath = util::ensureUserDir({".linglong", appID, "/cache"});
        mountMap.push_back(qMakePair(appCachePath, util::getUserFile(".cache")));

        mountMap.push_back(qMakePair(userRuntimeDir + "/dconf", userRuntimeDir + "/dconf"));

        mountMap.push_back(
            qMakePair(util::getUserFile(".config/user-dirs.dirs"), util::getUserFile(".config/user-dirs.dirs")));

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
        roMountMap.push_back(
            qMakePair(util::getUserFile(".local/share/fonts"), util::getUserFile(".local/share/fonts")));

        roMountMap.push_back(
            qMakePair(util::getUserFile(".config/fontconfig"), util::getUserFile(".config/fontconfig")));

        // mount fonts
        roMountMap.push_back(
            qMakePair(util::getUserFile(".local/share/fonts"), QString("/run/host/appearance/user-fonts")));

        // mount fonts cache
        mountMap.push_back(
            qMakePair(util::getUserFile(".cache/fontconfig"), QString("/run/host/appearance/user-fonts-cache")));

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
        auto appRef = package::Ref(q_ptr->package->ref);
        auto appBinaryPath = QStringList {"/opt/apps", appRef.id, "files/bin"}.join("/");
        auto appSharePath = QStringList {"/opt/apps", appRef.id, "files/share"}.join("/");
        if (useFlatpakRuntime) {
            appBinaryPath = "/app/bin";
        }
        r->process->env.push_back("PATH=" + appBinaryPath + ":" + getenv("PATH"));
        r->process->env.push_back("HOME=" + util::getUserFile(""));
        r->process->env.push_back("XDG_RUNTIME_DIR=" + userRuntimeDir);
        r->process->env.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path=" + util::jonsPath({userRuntimeDir, "bus"}));
        if (QString(getenv("XDG_DATA_DIRS")).isEmpty()) {
            r->process->env.push_back("XDG_DATA_DIRS=" + appSharePath + ":" + "/usr/local/share:/usr/share");
        } else {
            r->process->env.push_back("XDG_DATA_DIRS=" + appSharePath + ":" + getenv("XDG_DATA_DIRS"));
        }
        auto bypassENV = [&](const char *constEnv) {
            r->process->env.push_back(QString(constEnv) + "=" + getenv(constEnv));
        };

        QStringList envList = {"DISPLAY",
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
                               "XDG_SESSION_TYPE",
                               "CLUTTER_IM_MODULE",
                               "QT4_IM_MODULE",
                               "GTK_IM_MODULE"};

        for (auto &env : envList) {
            bypassENV(env.toStdString().c_str());
        }
        qDebug() << r->process->env;
        r->process->cwd = util::getUserFile("");

        QList<QList<quint64>> uidMaps = {
            {getuid(), 0, 1},
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
            {getgid(), 0, 1},
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

        if (!q->permission) {
            return 0;
        }

        QMap<QString, std::function<QString()>> replacementMap = {
            {"${HOME}", []() -> QString { return util::getUserFile(""); }},
        };

        auto pathPreprocess = [&](QString path) -> QString {
            auto keys = replacementMap.keys();
            for (const auto &key : keys) {
                path.replace(key, (replacementMap.value(key))());
            }
            return path;
        };

        //    TODO: debug mount for developer
        for (const auto &mount : q->permission->mounts) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {};
            auto component = mount.split(":");
            m.source = pathPreprocess(component.value(0));
            m.destination = pathPreprocess(component.value(1));
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        return 0;
    }

    bool useFlatpakRuntime = false;

    Container *container = nullptr;
    Runtime *r = nullptr;
    App *q_ptr = nullptr;

    Q_DECLARE_PUBLIC(App);
};

App::App(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new AppPrivate(this))
{
}

App *App::load(const QString &configFilepath, bool useFlatpakRuntime)
{
    qDebug() << "load conf yaml from" << configFilepath;
    QFile appConfig(configFilepath);
    appConfig.open(QIODevice::ReadOnly);
    App *app = nullptr;
    try {
        YAML::Node doc = YAML::Load(appConfig.readAll().toStdString());
        app = formYaml<App>(doc);
        // TODO: maybe set as an arg of init is better
        app->dd_ptr->useFlatpakRuntime = useFlatpakRuntime;
        app->dd_ptr->init();
    } catch (...) {
        qCritical() << "FIXME: load config failed, use default app config";
    }
    return app;
}

int App::start()
{
    Q_D(App);

    d->r->root->path = d->container->WorkingDirectory + "/root";
    util::ensureDir(d->r->root->path);

    d->prepare();

    // write pid file
    QFile pidFile(d->container->WorkingDirectory + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << d->r->root->path;
    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(d->r)).toJson();
    auto data = json.toStdString();

    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) {
        return EXIT_FAILURE;
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    pid_t boxPid = fork();
    if (boxPid < 0) {
        return -1;
    }

    if (0 == boxPid) {
        // child process
        (void)close(pipeEnds[1]);
        if (dup2(pipeEnds[0], LINGLONG) == -1) {
            return EXIT_FAILURE;
        }
        (void)close(pipeEnds[0]);

        char const *const args[] = {"/usr/bin/ll-box", LL_TOSTRING(LINGLONG), NULL};
        int ret = execvp(args[0], (char **)args);
        exit(ret);
    } else {
        close(pipeEnds[0]);
        write(pipeEnds[1], data.c_str(), data.size());
        close(pipeEnds[1]);

        d->container->PID = boxPid;
        // FIXME(interactive bash): if need keep interactive shell
        waitpid(boxPid, nullptr, 0);
    }

    return EXIT_SUCCESS;
}

Container *App::container() const
{
    Q_D(const App);
    return d->container;
}

App::~App() = default;
