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
#include "module/util/package_manager_param.h"
#include "module/package/info.h"
#include "module/repo/repo.h"
#include "module/flatpak/flatpak_manager.h"
#include "module/util/env.h"

#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

using namespace linglong;
namespace linglong {
namespace runtime {

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
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qCritical() << jsonFile.error() << jsonFile.errorString();
            return false;
        }
        auto json = QJsonDocument::fromJson(jsonFile.readAll());
        r = fromVariant<Runtime>(json.toVariant());
        r->setParent(q_ptr);

        container = new Container(q_ptr);
        container->create();

        return true;
    }

    int prepare()
    {
        Q_Q(App);

        // FIXME: get info from module/package
        auto runtimeRef = package::Ref(q->runtime->ref);
        QString runtimeRootPath = repo->rootOfLayer(runtimeRef);

        // FIXME: return error if files not exist
        auto fixRuntimePath = runtimeRootPath + "/files";
        if (!util::dirExists(fixRuntimePath)) {
            fixRuntimePath = runtimeRootPath;
        }

        auto appRef = package::Ref(q->package->ref);
        QString appRootPath = repo->rootOfLayer(appRef);

        stageRootfs(runtimeRef.appId, fixRuntimePath, appRef.appId, appRootPath);

        stageSystem();
        stageHost();
        stageUser(appRef.appId);
        stageMount();
        fixMount();

        auto envFilepath = container->workingDirectory + QString("/env");
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
        m.options = QStringList {"rbind"};
        m.source = envFilepath;
        m.destination = "/run/app/env";
        r->mounts.push_back(&m);

        // TODO: move to class package
        // find desktop file
        QDir applicationsDir(QStringList {appRootPath, "entries", "applications"}.join(QDir::separator()));
        auto desktopFilenameList = applicationsDir.entryList({"*.desktop"}, QDir::Files);
        if (useFlatpakRuntime) {
            desktopFilenameList = flatpak::FlatpakManager::instance()->getAppDesktopFileList(appRef.appId);
        }
        if (desktopFilenameList.length() <= 0) {
            return -1;
        }

        util::DesktopEntry desktopEntry(applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));

        if (r->process->args.isEmpty() && !desktopExec.isEmpty()) {
            r->process->args = util::parseExec(desktopExec);
        } else if (r->process->args.isEmpty()) {
            r->process->args = util::parseExec(desktopEntry.rawValue("Exec"));
        }
        // ll-cli run appId 获取的是原生desktop exec ,有的包含%F等参数，需要去掉
        // FIXME(liujianqiang):后续整改，参考下面链接
        // https://github.com/linuxdeepin/go-lib/blob/28a4ee3e8dbe6d6316d3b0053ee4bda1a7f63f98/appinfo/desktopappinfo/desktopappinfo.go
        // https://github.com/linuxdeepin/go-lib/commit/bd52a27688413e1273f8b516ef55dc472d7978fd
        auto indexNum = r->process->args.indexOf(QRegExp("^%\\w$"));
        if (indexNum != -1) {
            r->process->args.removeAt(indexNum);
        }

        qDebug() << "exec" << r->process->args;

        bool noDbusProxy = runParamMap.contains(linglong::util::KEY_NO_PROXY);
        if (!linglong::util::fileExists("/usr/bin/ll-dbus-proxy")) {
            noDbusProxy = true;
            qWarning() << "ll-dbus-proxy not installed";
        }
        QString sessionSocketPath = "";
        if (!noDbusProxy) {
            sessionSocketPath = linglong::util::createProxySocket("session-bus-proxy-XXXXXX");
            std::string pathString = sessionSocketPath.toStdString();
            unlink(pathString.c_str());
        }

        stageDBusProxy(sessionSocketPath, !noDbusProxy);
        qInfo() << "createProxySocket path:" << sessionSocketPath << ", noDbusProxy:" << noDbusProxy;
        stateDBusProxyArgs(!noDbusProxy, appRef.appId, sessionSocketPath);
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
            m.options = QStringList {"rbind"};
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount stageSystem" << m.source << m.destination;
        }
        return 0;
    }

    int stageRootfs(const QString &runtimeId, QString runtimeRootPath, const QString &appId, QString appRootPath) const
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
        r->annotations->containerRootPath = container->workingDirectory;

        if (fuseMount) {
            r->annotations->overlayfs = new AnnotationsOverlayfsRootfs(r->annotations);
            r->annotations->overlayfs->lowerParent =
                QStringList {container->workingDirectory, ".overlayfs", "lower_parent"}.join("/");
            r->annotations->overlayfs->upper =
                QStringList {container->workingDirectory, ".overlayfs", "upper"}.join("/");
            r->annotations->overlayfs->workdir =
                QStringList {container->workingDirectory, ".overlayfs", "workdir"}.join("/");
        } else {
            r->annotations->native = new AnnotationsNativeRootfs(r->annotations);
        }

        r->annotations->dbusProxyInfo = new DBusProxy(r->annotations);

        QList<QPair<QString, QString>> mountMap;

        if (useThinRuntime) {
            mountMap = {
                {"/usr", "/usr"},
                {"/etc", "/etc"},
                {runtimeRootPath + "/usr", "/runtime"},
            };

            // FIXME(iceyer): extract for wine, remove later
            if (fuseMount) {
                // NOTE: the override should be behind host /usr
                mountMap.push_back({runtimeRootPath + "/usr", "/usr"});
                mountMap.push_back({runtimeRootPath + "/opt/deepinwine", "/opt/deepinwine"});
                mountMap.push_back({runtimeRootPath + "/opt/deepin-wine6-stable", "/opt/deepin-wine6-stable"});
            }
        } else {
            if (useFlatpakRuntime) {
                runtimeRootPath = flatpak::FlatpakManager::instance()->getRuntimePath(appId);
            }
            // FIXME(iceyer): if runtime is empty, use the last
            if (runtimeRootPath.isEmpty()) {
                qCritical() << "mount runtime failed" << runtimeRootPath;
                return -1;
            }

            mountMap.push_back({runtimeRootPath, "/usr"});
        }

        if (useFlatpakRuntime) {
            appRootPath = flatpak::FlatpakManager::instance()->getAppPath(appId);
            mountMap.push_back({appRootPath, "/app"});
        } else {
            mountMap.push_back({appRootPath, "/opt/apps/" + appId});
            // TODO(iceyer): add doc for this or remove
            mountMap.push_back({QStringList {appRootPath, "files/lib"}.join("/"), "/run/app/lib"});
        }

        for (const auto &pair : mountMap) {
            auto m = new Mount(r);
            m->type = "bind";
            m->options = QStringList {"ro", "rbind"};
            m->source = pair.first;
            m->destination = pair.second;

            if (fuseMount) {
                r->annotations->overlayfs->mounts.push_back(m);
            } else {
                r->annotations->native->mounts.push_back(m);
            }
        }

        // TODO(iceyer): let application do this or add to doc
        auto appLdLibraryPath = QStringList {"/opt/apps", appId, "files/lib"}.join("/");
        if (useFlatpakRuntime) {
            appLdLibraryPath = "/app/lib";
        }

        // TODO(iceyer): support other arch, or just no arch?
        auto fixLdLibraryPath = QStringList {
            appLdLibraryPath,
            "/runtime/lib",
            "/runtime/lib/x86_64-linux-gnu",
            "/runtime/lib/i386-linux-gnu",
        };
        r->process->env.push_back("LD_LIBRARY_PATH=" + fixLdLibraryPath.join(":"));
        r->process->env.push_back(
            "QT_PLUGIN_PATH=/runtime/lib/x86_64-linux-gnu/qt5/plugins:/usr/lib/x86_64-linux-gnu/qt5/plugins");
        r->process->env.push_back(
            "QT_QPA_PLATFORM_PLUGIN_PATH=/runtime/lib/x86_64-linux-gnu/qt5/plugins/platforms:/usr/lib/x86_64-linux-gnu/qt5/plugins/platforms");
        r->process->env.push_back(
            "QTWEBENGINEPROCESS_PATH=/runtime/lib/x86_64-linux-gnu/qt5/libexec/QtWebEngineProcess");
        r->process->env.push_back(
            "QTWEBENGINERESOURCE_PATH=/runtime/share/qt5/translations:/runtime/share/qt5/resources");
        return 0;
    }

    int stageHost() const
    {
        QList<QPair<QString, QString>> roMountMap = {
            {"/etc/resolv.conf", "/run/host/network/etc/resolv.conf"},
            {"/run/resolvconf", "/run/resolvconf"},
            {"/usr/share/fonts", "/run/host/appearance/fonts"},
            {"/usr/share/locale/", "/usr/share/locale/"},
            {"/usr/lib/locale/", "/usr/lib/locale/"},
            {"/usr/share/themes", "/usr/share/themes"},
            {"/usr/share/icons", "/usr/share/icons"},
            {"/usr/share/zoneinfo", "/usr/share/zoneinfo"},
            {"/etc/localtime", "/run/host/etc/localtime"},
            {"/etc/machine-id", "/run/host/etc/machine-id"},
            {"/etc/machine-id", "/etc/machine-id"},
            {"/var", "/var"}, // FIXME: should we mount /var as "ro"?
            {"/var/cache/fontconfig", "/run/host/appearance/fonts-cache"},
        };

        for (auto const &item : QDir("/dev").entryInfoList({"nvidia*"}, QDir::AllEntries | QDir::System)) {
            roMountMap.push_back({item.canonicalFilePath(), item.canonicalFilePath()});
        }

        for (const auto &pair : roMountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"ro", "rbind"};
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        QList<QPair<QString, QString>> mountMap = {
            {"/tmp/.X11-unix", "/tmp/.X11-unix"}, // FIXME: only mount one DISPLAY
        };

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rbind"};
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        return 0;
    }

    void stateDBusProxyArgs(bool enable, const QString &appId, const QString &proxyPath)
    {
        r->annotations->dbusProxyInfo->appId = appId;
        r->annotations->dbusProxyInfo->enable = enable;
        if (!enable) {
            return;
        }
        r->annotations->dbusProxyInfo->busType = runParamMap[linglong::util::KEY_BUS_TYPE];
        r->annotations->dbusProxyInfo->proxyPath = proxyPath;
        // FIX to do load filter from yaml
        // FIX to do 加载用户配置参数（权限管限器上）
        // 添加cli command运行参数
        if (runParamMap.contains(linglong::util::KEY_FILTER_NAME)) {
            QString name = runParamMap[linglong::util::KEY_FILTER_NAME];
            if (!r->annotations->dbusProxyInfo->name.contains(name)) {
                r->annotations->dbusProxyInfo->name.push_back(name);
            }
        }
        if (runParamMap.contains(linglong::util::KEY_FILTER_PATH)) {
            QString path = runParamMap[linglong::util::KEY_FILTER_PATH];
            if (!r->annotations->dbusProxyInfo->path.contains(path)) {
                r->annotations->dbusProxyInfo->path.push_back(path);
            }
        }
        if (runParamMap.contains(linglong::util::KEY_FILTER_IFACE)) {
            QString interface = runParamMap[linglong::util::KEY_FILTER_IFACE];
            if (!r->annotations->dbusProxyInfo->interface.contains(interface)) {
                r->annotations->dbusProxyInfo->interface.push_back(interface);
            }
        }
    }

    // Fix to do 当前仅处理session bus
    int stageDBusProxy(const QString &socketPath, bool useDBusProxy = false)
    {
        QList<QPair<QString, QString>> mountMap;
        auto userRuntimeDir = QString("/run/user/%1/").arg(getuid());
        if (useDBusProxy) {
            // bind dbus-proxy-user, now use session bus
            mountMap.push_back(qMakePair(socketPath, userRuntimeDir + "/bus"));
            // fix to do, system bus in no-proxy mode
            mountMap.push_back(
                qMakePair(QString("/run/dbus/system_bus_socket"), QString("/run/dbus/system_bus_socket")));
        } else {
            mountMap.push_back(qMakePair(userRuntimeDir + "/bus", userRuntimeDir + "/bus"));
            mountMap.push_back(
                qMakePair(QString("/run/dbus/system_bus_socket"), QString("/run/dbus/system_bus_socket")));
        }
        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {};

            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        return 0;
    }

    int stageUser(const QString &appId) const
    {
        QList<QPair<QString, QString>> mountMap;

        // bind user data
        auto userRuntimeDir = QString("/run/user/%1").arg(getuid());
        {
            Mount &m = *new Mount(r);
            m.type = "tmpfs";
            m.options = QStringList {"nodev", "nosuid", "mode=700"};
            m.source = "tmpfs";
            m.destination = userRuntimeDir;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        // bind /run/usr/$(uid)/pulse
        mountMap.push_back(qMakePair(userRuntimeDir + "/pulse", userRuntimeDir + "/pulse"));

        // 处理摄像头挂载问题
        // bind /run/udev    /dev/video*
        if(linglong::util::dirExists("/run/udev")){
            mountMap.push_back(qMakePair(QString("run/udev"),QString("run/udev")));
        }
        auto videoFileList = QDir("/dev").entryList({"video*"},QDir::System);
        if(!videoFileList.isEmpty()){
            for(auto video : videoFileList){
                mountMap.push_back(qMakePair(QString("/dev/" + video),QString("/dev/" + video)));
            }
        }

        auto hostAppHome = util::ensureUserDir({".linglong", appId, "home"});
        mountMap.push_back(qMakePair(hostAppHome, util::getUserFile("")));

        // bind $(HOME)/.linglong/$(appId)
        auto appLinglongPath = util::ensureUserDir({".linglong", appId});
        mountMap.push_back(qMakePair(appLinglongPath, util::getUserFile(".linglong/" + appId)));

        auto appConfigPath = util::ensureUserDir({".linglong", appId, "/config"});
        mountMap.push_back(qMakePair(appConfigPath, util::getUserFile(".config")));

        auto appCachePath = util::ensureUserDir({".linglong", appId, "/cache"});
        mountMap.push_back(qMakePair(appCachePath, util::getUserFile(".cache")));

        mountMap.push_back(qMakePair(userRuntimeDir + "/dconf", userRuntimeDir + "/dconf"));

        mountMap.push_back(
            qMakePair(util::getUserFile(".config/user-dirs.dirs"), util::getUserFile(".config/user-dirs.dirs")));

        for (const auto &pair : mountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rbind"};

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
        roMountMap.push_back(
            qMakePair(util::getUserFile(".cache/fontconfig"), QString("/run/host/appearance/user-fonts-cache")));

        // mount dde-api
        // TODO ：主题相关，后续dde是否写成标准? 或者 此相关应用（如欢迎）不使用玲珑格式。
        auto ddeApiPath = util::ensureUserDir({".cache", "deepin", "dde-api"});
        roMountMap.push_back(qMakePair(ddeApiPath, ddeApiPath));

        // mount ~/.config/dconf
        // TODO: 所有应用主题相关设置数据保存在~/.config/dconf/user
        // 中，是否安全？一个应用沙箱中可以读取其他应用设置数据？ issues:
        // https://gitlabwh.uniontech.com/wuhan/v23/linglong/linglong/-/issues/72
        auto dconfPath = util::ensureUserDir({".config", "dconf"});
        roMountMap.push_back(qMakePair(dconfPath, dconfPath));

        QString xauthority = getenv("XAUTHORITY");
        roMountMap.push_back(qMakePair(xauthority, xauthority));

        for (const auto &pair : roMountMap) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"ro", "rbind"};
            m.source = pair.first;
            m.destination = pair.second;
            r->mounts.push_back(&m);
            qDebug() << "mount app" << m.source << m.destination;
        }

        //处理环境变量
        for (auto key : envMap.keys()) {
            if (linglong::util::envList.contains(key)) {
                r->process->env.push_back(key + "=" + envMap[key]);
            }
        }
        auto appRef = package::Ref(q_ptr->package->ref);
        auto appBinaryPath = QStringList {"/opt/apps", appRef.appId, "files/bin"}.join("/");
        if (useFlatpakRuntime) {
            appBinaryPath = "/app/bin";
        }

        //特殊处理env PATH
        if (envMap.contains("PATH")) {
            r->process->env.removeAt(r->process->env.indexOf("^PATH="));
            r->process->env.push_back("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":" + envMap["PATH"]);
        } else {
            r->process->env.push_back("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":" + getenv("PATH"));
        }

        //特殊处理env HOME
        if (!envMap.contains("HOME")) {
            r->process->env.push_back("HOME=" + util::getUserFile(""));
        }

        r->process->env.push_back("XDG_RUNTIME_DIR=" + userRuntimeDir);
        r->process->env.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path=" + util::jonsPath({userRuntimeDir, "bus"}));

        auto appSharePath = QStringList {"/opt/apps", appRef.appId, "files/share"}.join("/");
        if (useFlatpakRuntime) {
            appSharePath = "/app/share";
        }
        auto xdgDataDirs = QStringList {appSharePath, "/runtime/share"};
        xdgDataDirs.append(qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share"));
        r->process->env.push_back("XDG_DATA_DIRS=" + xdgDataDirs.join(":"));

        // add env XDG_CONFIG_HOME XDG_CACHE_HOME
        // set env XDG_CONFIG_HOME=$(HOME)/.linglong/$(appId)/config
        r->process->env.push_back("XDG_CONFIG_HOME=" + util::getUserFile(".linglong/" + appId + "/config"));
        // set env XDG_CACHE_HOME=$(HOME)/.linglong/$(appId)/cache
        r->process->env.push_back("XDG_CACHE_HOME=" + util::getUserFile(".linglong/" + appId + "/cache"));

        // set env XDG_DATA_HOME=$(HOME)/.linglong/$(appId)/share
        r->process->env.push_back("XDG_DATA_HOME=" + util::getUserFile(".linglong/" + appId + "/share"));

        qDebug() << r->process->env;
        r->process->cwd = util::getUserFile("");

        QList<QList<quint64>> uidMaps = {
            {getuid(), 0, 1},
        };
        for (auto const &uidMap : uidMaps) {
            Q_ASSERT(uidMap.size() == 3);
            auto idMap = new IdMap(r->linux);
            idMap->hostId = uidMap.value(0);
            idMap->containerId = uidMap.value(1);
            idMap->size = uidMap.value(2);
            r->linux->uidMappings.push_back(idMap);
        }

        QList<QList<quint64>> gidMaps = {
            {getgid(), 0, 1},
        };
        for (auto const &gidMap : gidMaps) {
            Q_ASSERT(gidMap.size() == 3);
            auto idMap = new IdMap(r->linux);
            idMap->hostId = gidMap.value(0);
            idMap->containerId = gidMap.value(1);
            idMap->size = gidMap.value(2);
            r->linux->gidMappings.push_back(idMap);
        }

        return 0;
    }

    int stageMount()
    {
        Q_Q(const App);

        if (!q->permissions || q->permissions->mounts.isEmpty()) {
            // not found permission static mount
            return 0;
        }

        // static mount
        for (const auto &mount : q->permissions->mounts) {
            auto &m = *new Mount(r);

            // illegal mount rules
            if (mount->source.isEmpty() || mount->destination.isEmpty()) {
                continue;
            }
            // fix default type
            if (mount->type.isEmpty()) {
                m.type = "bind";
            } else {
                m.type = mount->type;
            }

            // fix default options
            if (mount->options.isEmpty()) {
                m.options = QStringList({"ro", "rbind"});
            } else {
                m.options = mount->options.split(",");
            }

            m.source = mount->source;
            m.destination = mount->destination;
            r->mounts.push_back(&m);

            qDebug() << "add static mount:" << mount->source << " => " << mount->destination;
        }

        return 0;
    }

    int fixMount()
    {
        Q_Q(const App);
        // 360浏览器需要/apps-data/private/com.360.browser-stable目录可写
        // todo:后续360整改
        // 参考：https://gitlabwh.uniontech.com/wuhan/se/deepin-specifications/-/blob/master/unstable/%E5%BA%94%E7%94%A8%E6%95%B0%E6%8D%AE%E7%9B%AE%E5%BD%95%E8%A7%84%E8%8C%83.md
        auto appRef = linglong::package::Ref(q->package->ref);
        //获取runtime安装路径
        auto runtimeRef = linglong::package::Ref(q->runtime->ref);
        auto runtimeInstallRoot = repo->rootOfLayer(runtimeRef);

        if (QString("com.360.browser-stable") == appRef.appId) {
            // FIXME: 需要一个所有用户都有可读可写权限的目录
            QString appDataPath = util::getUserFile(".linglong/" + appRef.appId + "/share/appdata");
            linglong::util::ensureDir(appDataPath);
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rw", "rbind"};
            m.source = appDataPath;
            m.destination = "/apps-data/private/com.360.browser-stable";
            r->mounts.push_back(&m);
        }

        // 临时默认挂载用户相关目录
        // todo: 后续加权限后整改
        // Fixme: modify static mount that after this comment code.
        auto usrDirList =
            QStringList {"Desktop", "Documents", "Downloads", "Music", "Pictures", "Videos", ".Public", ".Templates"};
        for (auto dir : usrDirList) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rw", "rbind"};
            m.source = util::getUserFile(dir);
            m.destination = util::getUserFile(dir);
            r->mounts.push_back(&m);
        }

        //挂载runtime的xdg-open和xdg-email到沙箱/usr/bin下
        auto xdgFileDirList = QStringList {"xdg-open", "xdg-email"};
        for (auto dir : xdgFileDirList) {
            Mount &m = *new Mount(r);
            m.type = "bind";
            m.options = QStringList {"rbind"};
            m.source = runtimeInstallRoot + "/usr/bin/" + dir;
            m.destination = "/usr/bin/" + dir;
            r->mounts.push_back(&m);
        }
        return 0;
    }

    // FIXME: none static
    static QString loadConfig(linglong::repo::Repo *repo, const QString &appId, const QString &appVersion,
                              bool isFlatpakApp = false)
    {
        util::ensureUserDir({".linglong", appId});

        auto configPath = getUserFile(QString("%1/%2/app.yaml").arg(".linglong", appId));

        // create yaml form info
        // auto appRoot = LocalRepo::get()->rootOfLatest();
        auto latestAppRef = repo->latestOfRef(appId, appVersion);

        auto appInstallRoot = repo->rootOfLayer(latestAppRef);

        auto appInfo = appInstallRoot + "/info.json";
        // 判断是否存在
        if (!isFlatpakApp && !fileExists(appInfo)) {
            qCritical() << appInfo << " not exist";
            return "";
        }

        // create a yaml config from json
        auto info = util::loadJSON<package::Info>(appInfo);

        if (info->runtime.isEmpty()) {
            // FIXME: return error is not exist

            // thin runtime
            info->runtime = "org.deepin.Runtime/20/x86_64";

            // full runtime
            // info->runtime = "deepin.Runtime.Sdk/23/x86_64";
        }

        package::Ref runtimeRef(info->runtime);

        QMap<QString, QString> variables = {
            {"APP_REF", latestAppRef.toLocalRefString()},
            {"RUNTIME_REF", runtimeRef.toLocalRefString()},
        };

        // TODO: remove to util module as file_template.cpp

        // permission load
        QMap<QString, QString> permissionMountsMap;

        const package::User *permissionUserMounts = nullptr;
        // old info.json load permission failed
        permissionUserMounts = info->permissions && info->permissions->filesystem && info->permissions->filesystem->user
                                   ? info->permissions->filesystem->user
                                   : nullptr;

        if (permissionUserMounts != nullptr) {
            auto permVariant = toVariant<linglong::package::User>(permissionUserMounts);
            auto loadPermissionMap = permVariant.toMap();
            if (!loadPermissionMap.empty()) {
                QStringList userTypeList = linglong::util::getXdgUserDir();
                for (const auto &it : loadPermissionMap.keys()) {
                    auto itValue = loadPermissionMap.value(it).toString();
                    if (itValue != "" && (itValue == "r" || itValue == "rw" || itValue == "ro")
                        && userTypeList.indexOf(it) != -1) {
                        permissionMountsMap.insert(it, itValue);
                    }
                }
            }
        }

        QFile templateFile(":/app.yaml");
        templateFile.open(QIODevice::ReadOnly);
        auto templateData = templateFile.readAll();
        foreach (auto const &k, variables.keys()) {
            templateData.replace(QString("@%1@").arg(k).toLocal8Bit(), variables.value(k).toLocal8Bit());
        }

        // permission data to yaml
        QString permissionMountsData;
        if (!permissionMountsMap.empty()) {
            permissionMountsData += "\n\npermissions:";
            permissionMountsData += "\n  mounts:";
            for (auto const &it : permissionMountsMap.keys()) {
                auto sourceDir = util::getXdgDir(it);
                if (sourceDir.first) {
                    QString optionStr = permissionMountsMap.value(it) == "rw" ? "rw,rbind" : "";
                    if (optionStr == "") {
                        permissionMountsData += QString("\n    - source: %2\n      destination: %3")
                                                    .arg(sourceDir.second)
                                                    .arg(sourceDir.second);
                    } else {
                        permissionMountsData +=
                            QString("\n    - type: bind\n      options: %1\n      source: %2\n      destination: %3")
                                .arg(optionStr)
                                .arg(sourceDir.second)
                                .arg(sourceDir.second);
                    }
                } else {
                    continue;
                }
            }
            permissionMountsData += "\n";
        }
        templateFile.close();

        QFile configFile(configPath);
        configFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        configFile.write(templateData);
        if (!permissionMountsData.isEmpty())
            configFile.write(permissionMountsData.toLocal8Bit());
        configFile.close();

        return configPath;
    }

    bool useFlatpakRuntime = false;
    QString desktopExec = nullptr;
    ParamStringMap envMap;
    ParamStringMap runParamMap;

    Container *container = nullptr;
    Runtime *r = nullptr;
    App *q_ptr = nullptr;

    repo::Repo *repo;

    Q_DECLARE_PUBLIC(App);
};

App::App(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new AppPrivate(this))
{
}

App *App::load(linglong::repo::Repo *repo, const package::Ref &ref, const QString &desktopExec, bool useFlatpakRuntime)
{
    QString configPath = AppPrivate::loadConfig(repo, ref.appId, ref.version, useFlatpakRuntime);
    if (!fileExists(configPath)) {
        return nullptr;
    }

    QFile appConfig(configPath);
    appConfig.open(QIODevice::ReadOnly);

    qDebug() << "load conf yaml from" << configPath;

    App *app = nullptr;
    try {
        auto data = QString::fromLocal8Bit(appConfig.readAll());
        qDebug() << data;
        YAML::Node doc = YAML::Load(data.toStdString());
        app = formYaml<App>(doc);

        qDebug() << app << app->runtime << app->package << app->version;
        // TODO: maybe set as an arg of init is better
        app->dd_ptr->useFlatpakRuntime = useFlatpakRuntime;
        app->dd_ptr->desktopExec = desktopExec;
        app->dd_ptr->repo = repo;
        app->dd_ptr->init();
    } catch (...) {
        qCritical() << "FIXME: load config failed, use default app config";
    }
    return app;
}

int App::start()
{
    Q_D(App);

    d->r->root->path = d->container->workingDirectory + "/root";
    util::ensureDir(d->r->root->path);

    d->prepare();

    // write pid file
    QFile pidFile(d->container->workingDirectory + QString("/%1.pid").arg(getpid()));
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

        d->container->pid = boxPid;
        // FIXME(interactive bash): if need keep interactive shell
        waitpid(boxPid, nullptr, 0);
        // FIXME to do 删除代理socket临时文件
    }

    return EXIT_SUCCESS;
}

Container *App::container() const
{
    Q_D(const App);
    return d->container;
}

void App::saveUserEnvList(const QStringList &userEnvList)
{
    Q_D(App);
    for (auto env : userEnvList) {
        auto sepPos = env.indexOf("=");
        d->envMap.insert(env.left(sepPos), env.right(env.length() - sepPos - 1));
    }
}

void App::setAppParamMap(const ParamStringMap& paramMap)
{
    Q_D(App);
    d->runParamMap = paramMap;
}
App::~App() = default;

} // namespace runtime
} // namespace linglong
