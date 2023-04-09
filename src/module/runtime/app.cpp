/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app.h"

#include "module/dbus_ipc/package_manager_param.h"
#include "module/package/info.h"
#include "module/repo/repo.h"
#include "module/runtime/app_config.h"
#include "module/util/desktop_entry.h"
#include "module/util/env.h"
#include "module/util/file.h"
#include "module/util/qserializer/json.h"
#include "module/util/qserializer/yaml.h"
#include "module/util/version/version.h"
#include "module/util/xdg.h"

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include <mutex>

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

static void initQResource()
{
    Q_INIT_RESOURCE(app_configs);
}

namespace linglong {
namespace runtime {

QSERIALIZER_IMPL(App);
QSERIALIZER_IMPL(AppPermission);
QSERIALIZER_IMPL(Layer);
QSERIALIZER_IMPL(MountYaml);

namespace PrivateAppInit {
int init()
{
    static std::once_flag flag;
    std::call_once(flag, initQResource);
    return 0;
}
} // namespace PrivateAppInit

enum RunArch {
    UNKNOWN,
    ARM64,
    X86_64,
};

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
            qFatal("builtin oci configuration file missing, check qrc");
            return false;
        }
        auto json = QJsonDocument::fromJson(jsonFile.readAll());
        jsonFile.close();
        r = json.toVariant().value<QSharedPointer<Runtime>>();
        r->setParent(q_ptr);

        container.reset(new Container(q_ptr));
        container->create(q_ptr->package->ref);

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

        stageRootfs(fixRuntimePath, appRef.appId, appRootPath);

        stageSystem();
        stageUser(appRef.appId);
        stageMount();
        stageHost();
        fixMount(fixRuntimePath, appRef.appId);

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

        QSharedPointer<Mount> m(new Mount);
        m->type = "bind";
        m->options = QStringList{ "rbind" };
        m->source = envFilepath;
        m->destination = "/run/app/env";
        r->mounts.push_back(m);

        // TODO: move to class package
        // find desktop file
        QDir applicationsDir(
                QStringList{ appRootPath, "entries", "applications" }.join(QDir::separator()));
        auto desktopFilenameList = applicationsDir.entryList({ "*.desktop" }, QDir::Files);
        if (desktopFilenameList.length() <= 0) {
            return -1;
        }

        util::DesktopEntry desktopEntry(
                applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));

        // 当执行ll-cli run appid时，从entries目录获取执行参数，同时兼容旧的outputs打包模式。
        QStringList tmpArgs;
        QStringList execArgs;
        if (util::dirExists(
                    QStringList{ appRootPath, "outputs", "share" }.join(QDir::separator()))) {
            execArgs = util::parseExec(desktopEntry.rawValue("Exec"));
        } else {
            tmpArgs = util::parseExec(desktopEntry.rawValue("Exec"));
            // 移除 ll-cli run  appid --exec 参数
            for (auto i = tmpArgs.indexOf(QRegExp("^--exec$")) + 1; i < tmpArgs.length(); ++i) {
                execArgs << tmpArgs[i];
            }
        }

        if (r->process->args.isEmpty() && !desktopExec.isEmpty()) {
            r->process->args = util::splitExec(desktopExec);
        } else if (r->process->args.isEmpty()) {
            r->process->args = execArgs;
        }
        // ll-cli run appId 获取的是原生desktop exec ,有的包含%F等参数，需要去掉
        // FIXME(liujianqiang):后续整改，参考下面链接
        // https://github.com/linuxdeepin/go-lib/blob/28a4ee3e8dbe6d6316d3b0053ee4bda1a7f63f98/appinfo/desktopappinfo/desktopappinfo.go
        // https://github.com/linuxdeepin/go-lib/commit/bd52a27688413e1273f8b516ef55dc472d7978fd
        auto indexNum = r->process->args.indexOf(QRegExp("^%\\w$"));
        if (indexNum != -1) {
            r->process->args.removeAt(indexNum);
        }

        // desktop文件修改或者添加环境变量支持
        tmpArgs = util::parseExec(desktopEntry.rawValue("Exec"));
        auto indexOfEnv = tmpArgs.indexOf(QRegExp("^env$"));
        if (indexOfEnv != -1) {
            auto env = tmpArgs[indexOfEnv + 1];
            auto sepPos = env.indexOf("=");
            auto indexResult = r->process->env.indexOf(QRegExp("^" + env.left(sepPos + 1) + ".*"));
            if (indexResult != -1) {
                r->process->env.removeAt(indexResult);
                r->process->env.push_back(env);
            } else {
                r->process->env.push_back(env);
            }
        }

        qDebug() << "exec" << r->process->args;

        bool noDbusProxy = runParamMap.contains(linglong::util::kKeyNoProxy);
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
        qInfo() << "createProxySocket path:" << sessionSocketPath
                << ", noDbusProxy:" << noDbusProxy;
        stateDBusProxyArgs(!noDbusProxy, appRef.appId, sessionSocketPath);
        return 0;
    }

    int stageSystem() const
    {
        QList<QPair<QString, QString>> mountMap;
        mountMap = {
            { "/dev/dri", "/dev/dri" },
            { "/dev/snd", "/dev/snd" },
        };

        for (const auto &pair : mountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount stageSystem" << m->source << m->destination;
        }
        return 0;
    }

    int stageRootfs(QString runtimeRootPath, const QString &appId, QString appRootPath) const
    {
        // 使用linglong runtime标志
        bool useThinRuntime = true;
        // overlay 挂载标志
        bool fuseMount = false;
        // wine 应用挂载标志
        bool wineMount = false;
        // 通过info.json中overlay挂载标志
        bool specialCase = false;
        // other sys overlay 挂载标志
        bool otherSysMount = false;

        // if use wine runtime, mount with fuse
        // FIXME(iceyer): use info.json to decide use fuse or not
        if (runtimeRootPath.contains("org.deepin.Wine")) {
            wineMount = true;
            fuseMount = true;
        }

        if (useFlatpakRuntime) {
            fuseMount = false;
            useThinRuntime = false;
        }

        r->annotations.reset(new Annotations);
        r->annotations->containerRootPath = container->workingDirectory;

        // 通过info.json文件判断是否要overlay mount
        auto appInfoFile = appRootPath + "/info.json";

        QSharedPointer<package::Info> info;
        if (util::fileExists(appInfoFile)) {
            info = util::loadJson<package::Info>(appInfoFile);
            if (info->overlayfs && info->overlayfs->mounts.size() > 0) {
                fuseMount = true;
                specialCase = true;
            }
        }

        // 转化特殊变量
        // 获取环境变量LINGLONG_ROOT
        auto linglongRootPath = util::getLinglongRootPath();
        QMap<QString, QString> variables = {
            { "APP_ROOT_PATH", appRootPath },
            { "RUNTIME_ROOT_PATH", runtimeRootPath },
            { "APP_ROOT_SHARE_PATH", sysLinglongInstalltions },
            { "LINGLONG_ROOT", linglongRootPath },
        };
        auto getPath = [&](QString &path) -> QString {
            for (auto key : variables.keys()) {
                path.replace(QString("$%1").arg(key).toLocal8Bit(),
                             variables.value(key).toLocal8Bit());
            }
            return path;
        };

        // 通过runtime info.json文件获取basicsRootPath路径
        auto runtimeRef = package::Ref(q_ptr->runtime->ref);
        QString runtimePath = repo->rootOfLayer(runtimeRef);
        auto runtimeInfoFile = runtimePath + "/info.json";
        // basics usr
        QString basicsUsrRootPath = "";
        // basics etc
        QString basicsEtcRootPath = "";
        if (!linglong::util::isDeepinSysProduct() && useThinRuntime) {
            QSharedPointer<package::Info> runtimeInfo;
            if (util::fileExists(runtimeInfoFile)) {
                runtimeInfo = util::loadJson<package::Info>(runtimeInfoFile);
                if (!runtimeInfo->runtime.isEmpty()) {
                    basicsUsrRootPath =
                            linglongRootPath + "/layers/" + runtimeInfo->runtime + "/files/usr";
                    basicsEtcRootPath =
                            linglongRootPath + "/layers/" + runtimeInfo->runtime + "/files/etc";
                    fuseMount = true;
                    otherSysMount = true;
                }
            }
        }

        // 使用overlayfs挂载debug调试符号
        auto debugRef = package::Ref(q_ptr->package->ref);
        qDebug() << "stageRootfs debugRef " << q_ptr->package->ref;
        if ("devel" == debugRef.module) {
            fuseMount = true;
        }

        if (fuseMount) {
            r->annotations->overlayfs.reset(new AnnotationsOverlayfsRootfs);
            r->annotations->overlayfs->lowerParent =
                    QStringList{ container->workingDirectory, ".overlayfs", "lower_parent" }.join(
                            "/");
            r->annotations->overlayfs->upper =
                    QStringList{ container->workingDirectory, ".overlayfs", "upper" }.join("/");
            r->annotations->overlayfs->workdir =
                    QStringList{ container->workingDirectory, ".overlayfs", "workdir" }.join("/");
        } else {
            r->annotations->native.reset(new AnnotationsNativeRootfs);
        }

        r->annotations->dbusProxyInfo.reset(new DBusProxy());

        QList<QPair<QString, QString>> mountMap;

        if (useThinRuntime) {
            mountMap = {
                { "/usr", "/usr" },
                { "/etc", "/etc" },
                { runtimeRootPath, "/runtime" },
                { "/usr/share/locale/", "/usr/share/locale/" },
            };

            qDebug() << "stageRootfs runtimeRootPath:" << runtimeRootPath
                     << "appRootPath:" << appRootPath;
            // appRootPath/devel/files/debug /usr/lib/debug/opt/apps/appid/files 挂载调试符号
            if ("devel" == debugRef.module) {
                mountMap.push_back({ appRootPath + "/devel/files/debug",
                                     "/usr/lib/debug/opt/apps/" + debugRef.appId + "/files" });
                // runtime 只用挂载devel/files/debug 目录
                mountMap.push_back({ runtimeRootPath.left(runtimeRootPath.length()
                                                          - QString("/files").length())
                                             + "/devel/files/debug",
                                     "/usr/lib/debug/runtime" });
            }

            // FIXME(iceyer): extract for wine, remove later
            if (fuseMount && wineMount) {
                // NOTE: the override should be behind host /usr
                mountMap.push_back({ runtimeRootPath + "/bin", "/usr/bin" });
                mountMap.push_back({ runtimeRootPath + "/include", "/usr/include" });
                mountMap.push_back({ runtimeRootPath + "/lib", "/usr/lib" });
                mountMap.push_back({ runtimeRootPath + "/sbin", "/usr/sbin" });
                mountMap.push_back({ runtimeRootPath + "/share", "/usr/share" });
                mountMap.push_back({ runtimeRootPath + "/opt/deepinwine", "/opt/deepinwine" });
                mountMap.push_back({ runtimeRootPath + "/opt/deepin-wine6-stable",
                                     "/opt/deepin-wine6-stable" });
            }
            // overlay mount 通过info.json
            if (fuseMount && specialCase) {
                for (auto mount : info->overlayfs->mounts) {
                    mountMap.push_back({ getPath(mount->source), getPath(mount->destination) });
                }
            }
            // overlay mount basics
            if (fuseMount && otherSysMount) {
                mountMap.push_back({ basicsUsrRootPath, "/usr" });
                mountMap.push_back({ basicsEtcRootPath, "/etc" });
            }
        } else {
            // FIXME(iceyer): if runtime is empty, use the last
            if (runtimeRootPath.isEmpty()) {
                qCritical() << "mount runtime failed" << runtimeRootPath;
                return -1;
            }

            mountMap.push_back({ runtimeRootPath, "/usr" });
        }

        for (const auto &pair : mountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "ro", "rbind" };
            m->source = pair.first;
            m->destination = pair.second;

            if (fuseMount) {
                // overlay mount 顺序是反向的
                if (wineMount) {
                    // wine应用先保持不变（会导致wine应用运行失败），后续整改
                    r->annotations->overlayfs->mounts.push_back(m);
                } else {
                    // ll-box overlay失效，待修复后改为push_front（gnome应用运行失效）
                    r->annotations->overlayfs->mounts.push_back(m);
                }
            } else {
                r->annotations->native->mounts.push_back(m);
            }
        }

        // 读写挂载/opt,有的应用需要读写自身携带的资源文件。eg:云看盘
        QString appMountPath = "";
        appMountPath = "/opt/apps/" + appId;
        QSharedPointer<Mount> m(new Mount);
        m->type = "bind";
        m->options = QStringList{ "rw", "rbind" };
        m->source = appRootPath;
        m->destination = appMountPath;

        if (fuseMount) {
            // overlay mount 顺序是反向的
            if (wineMount) {
                // wine应用先保持不变（会导致wine应用运行失败），后续整改
                r->annotations->overlayfs->mounts.push_back(m);
            } else {
                // ll-box overlay失效，待修复后改为push_front（gnome应用运行失效）
                r->annotations->overlayfs->mounts.push_back(m);
            }
        } else {
            r->annotations->native->mounts.push_back(m);
        }

        // TODO(iceyer): let application do this or add to doc
        auto appLdLibraryPath = QStringList{ "/opt/apps", appId, "files/lib" }.join("/");
        if (useFlatpakRuntime) {
            appLdLibraryPath = "/app/lib";
        }

        // todo: 代码冗余，后续整改，配置文件？
        QStringList fixLdLibraryPath;
        RunArch runArch;
        auto appRef = package::Ref(q_ptr->package->ref);
        if (appRef.arch == "arm64") {
            runArch = ARM64;
        } else if (appRef.arch == "x86_64") {
            runArch = X86_64;
        } else {
            runArch = UNKNOWN;
        }
        switch (runArch) {
        case ARM64:
            fixLdLibraryPath = QStringList{
                appLdLibraryPath, appLdLibraryPath + "/aarch64-linux-gnu",
                "/runtime/lib",   "/runtime/lib/aarch64-linux-gnu",
                "/usr/lib",       "/usr/lib/aarch64-linux-gnu",
            };
            r->process->env.push_back("QT_PLUGIN_PATH=/opt/apps/" + appId
                                      + "/files/plugins:/runtime/lib/aarch64-linux-gnu/qt5/"
                                        "plugins:/usr/lib/aarch64-linux-gnu/qt5/plugins");
            r->process->env.push_back(
                    "QT_QPA_PLATFORM_PLUGIN_PATH=/opt/apps/" + appId
                    + "/files/plugins/platforms:/runtime/lib/aarch64-linux-gnu/qt5/plugins/"
                      "platforms:/usr/lib/aarch64-linux-gnu/qt5/plugins/platforms");
            if (!fuseMount) {
                r->process->env.push_back("GST_PLUGIN_PATH=/opt/apps/" + appId
                                          + "/files/lib/aarch64-linux-gnu/gstreamer-1.0");
            }
            break;
        case X86_64:
            fixLdLibraryPath = QStringList{
                appLdLibraryPath,
                appLdLibraryPath + "/x86_64-linux-gnu",
                "/runtime/lib",
                "/runtime/lib/x86_64-linux-gnu",
                "/runtime/lib/i386-linux-gnu",
                "/usr/lib",
                "/usr/lib/x86_64-linux-gnu",
            };
            r->process->env.push_back("QT_PLUGIN_PATH=/opt/apps/" + appId
                                      + "/files/plugins:/runtime/lib/x86_64-linux-gnu/qt5/plugins:/"
                                        "usr/lib/x86_64-linux-gnu/qt5/plugins");
            r->process->env.push_back(
                    "QT_QPA_PLATFORM_PLUGIN_PATH=/opt/apps/" + appId
                    + "/files/plugins/platforms:/runtime/lib/x86_64-linux-gnu/qt5/plugins/"
                      "platforms:/usr/lib/x86_64-linux-gnu/qt5/plugins/platforms");
            if (!fuseMount) {
                r->process->env.push_back("GST_PLUGIN_PATH=/opt/apps/" + appId
                                          + "/files/lib/x86_64-linux-gnu/gstreamer-1.0");
            }
            break;
        default:
            qInfo() << "no supported arch :" << appRef.arch;
            return -1;
        }

        r->process->env.push_back("LD_LIBRARY_PATH=" + fixLdLibraryPath.join(":"));
        return 0;
    }

    int stageHost() const
    {
        QList<QPair<QString, QString>> roMountMap = {
            { "/etc/resolv.conf", "/run/host/network/etc/resolv.conf" },
            { "/run/resolvconf", "/run/resolvconf" },
            { "/usr/share/fonts", "/run/host/appearance/fonts" },
            { "/usr/lib/locale/", "/usr/lib/locale/" },
            { "/usr/share/themes", "/usr/share/themes" },
            { "/usr/share/icons", "/usr/share/icons" },
            { "/usr/share/zoneinfo", "/usr/share/zoneinfo" },
            { "/etc/localtime", "/run/host/etc/localtime" },
            { "/etc/machine-id", "/run/host/etc/machine-id" },
            { "/etc/machine-id", "/etc/machine-id" },
            { "/var", "/var" }, // FIXME: should we mount /var as "ro"?
            { "/var/cache/fontconfig", "/run/host/appearance/fonts-cache" },
        };

        // bind /dev/nvidia*
        for (auto const &item :
             QDir("/dev").entryInfoList({ "nvidia*" }, QDir::AllEntries | QDir::System)) {
            roMountMap.push_back({ item.canonicalFilePath(), item.canonicalFilePath() });
        }

        for (const auto &pair : roMountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "ro", "rbind" };
            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
        }

        QList<QPair<QString, QString>> mountMap = {
            { "/tmp/.X11-unix", "/tmp/.X11-unix" }, // FIXME: only mount one DISPLAY
        };

        for (const auto &pair : mountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
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
        r->annotations->dbusProxyInfo->busType = runParamMap[linglong::util::kKeyBusType];
        r->annotations->dbusProxyInfo->proxyPath = proxyPath;
        // FIX to do load filter from yaml
        // FIX to do 加载用户配置参数（权限管限器上）
        // 添加cli command运行参数
        if (runParamMap.contains(linglong::util::kKeyFilterName)) {
            QString name = runParamMap[linglong::util::kKeyFilterName];
            if (!r->annotations->dbusProxyInfo->name.contains(name)) {
                r->annotations->dbusProxyInfo->name.push_back(name);
            }
        }
        if (runParamMap.contains(linglong::util::kKeyFilterPath)) {
            QString path = runParamMap[linglong::util::kKeyFilterPath];
            if (!r->annotations->dbusProxyInfo->path.contains(path)) {
                r->annotations->dbusProxyInfo->path.push_back(path);
            }
        }
        if (runParamMap.contains(linglong::util::kKeyFilterIface)) {
            QString interface = runParamMap[linglong::util::kKeyFilterIface];
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
            mountMap.push_back(qMakePair(QString("/run/dbus/system_bus_socket"),
                                         QString("/run/dbus/system_bus_socket")));
        } else {
            mountMap.push_back(qMakePair(userRuntimeDir + "/bus", userRuntimeDir + "/bus"));
            mountMap.push_back(qMakePair(QString("/run/dbus/system_bus_socket"),
                                         QString("/run/dbus/system_bus_socket")));
        }
        for (const auto &pair : mountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{};

            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
        }

        return 0;
    }

    int stageUser(const QString &appId) const
    {
        QList<QPair<QString, QString>> mountMap;

        // bind user data
        auto userRuntimeDir = QString("/run/user/%1").arg(getuid());
        {
            QSharedPointer<Mount> m(new Mount);
            m->type = "tmpfs";
            m->options = QStringList{ "nodev", "nosuid", "mode=700" };
            m->source = "tmpfs";
            m->destination = userRuntimeDir;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
        }

        // bind /run/usr/$(uid)/pulse
        mountMap.push_back(qMakePair(userRuntimeDir + "/pulse", userRuntimeDir + "/pulse"));

        // bind /run/user/uid/gvfs
        mountMap.push_back(qMakePair(userRuntimeDir + "/gvfs", userRuntimeDir + "/gvfs"));

        // 处理摄像头挂载问题
        // bind /run/udev    /dev/video*
        if (linglong::util::dirExists("/run/udev")) {
            mountMap.push_back(qMakePair(QString("/run/udev"), QString("/run/udev")));
        }
        auto videoFileList = QDir("/dev").entryList({ "video*" }, QDir::System);
        if (!videoFileList.isEmpty()) {
            for (auto video : videoFileList) {
                mountMap.push_back(qMakePair(QString("/dev/" + video), QString("/dev/" + video)));
            }
        }

        // bind /run/user/$(uid)/wayland-0
        for (auto const &item :
             QDir(userRuntimeDir).entryInfoList({ "wayland*" }, QDir::AllEntries | QDir::System)) {
            mountMap.push_back({ item.canonicalFilePath(), item.canonicalFilePath() });
        }

        // 挂载宿主机home目录
        auto hostHome = util::getUserFile("");
        mountMap.push_back(qMakePair(hostHome, hostHome));

        // FIXME(black_desk): this should not be supported in linglong, remove it later.
        // 挂载/persistent/home/$username or /data/home/$username
        auto linglongRootPath = linglong::util::getLinglongRootPath();
        if ("/persistent/linglong" == linglongRootPath) {
            mountMap.push_back(qMakePair("/persistent" + hostHome, "/persistent" + hostHome));
        } else if ("/data/linglong" == linglongRootPath) {
            mountMap.push_back(qMakePair("/data" + hostHome, "/data" + hostHome));
        }

        // home目录下隐藏目录如.config .cache .local .deepinwine 需要特殊处理
        auto appLinglongPath = util::ensureUserDir({ ".linglong", appId });
        mountMap.push_back(qMakePair(appLinglongPath, util::getUserFile(".linglong/" + appId)));

        // .local/share/icons需要被沙箱外程序访问，临时挂载到.linglong/下
        auto appLocalIconPath = util::ensureUserDir({ ".linglong", appId, "/share/icons" });
        mountMap.push_back(qMakePair(util::getUserFile(".local/share/icons"), appLocalIconPath));

        auto appLocalDataPath = util::ensureUserDir({ ".linglong", appId, "/share" });
        mountMap.push_back(qMakePair(appLocalDataPath, util::getUserFile(".local/share")));

        mountMap.push_back(qMakePair(appLocalIconPath, util::getUserFile(".local/share/icons")));

        auto appConfigPath = util::ensureUserDir({ ".linglong", appId, "/config" });
        mountMap.push_back(qMakePair(appConfigPath, util::getUserFile(".config")));

        auto appCachePath = util::ensureUserDir({ ".linglong", appId, "/cache" });
        mountMap.push_back(qMakePair(appCachePath, util::getUserFile(".cache")));

        // bind $HOME/.deepinwine
        auto deepinWinePath = util::ensureUserDir({ ".deepinwine" });
        mountMap.push_back(qMakePair(deepinWinePath, util::getUserFile(".deepinwine")));

        // FIXME: should not bind mount dconf
        mountMap.push_back(qMakePair(userRuntimeDir + "/dconf", userRuntimeDir + "/dconf"));

        // bind systemd/user to box
        // create ~/.config/systemd/user
        util::ensureUserDir({ ".config/systemd/user" });
        auto appUserSystemdPath =
                util::ensureUserDir({ ".linglong", appId, "/config/systemd/user" });
        mountMap.push_back(
                qMakePair(util::getUserFile(".config/systemd/user"), appUserSystemdPath));

        // mount .config/user-dirs.dirs todo:移除挂载到~/.config下？
        mountMap.push_back(qMakePair(util::getUserFile(".config/user-dirs.dirs"),
                                     util::getUserFile(".config/user-dirs.dirs")));
        mountMap.push_back(qMakePair(util::getUserFile(".config/user-dirs.dirs"),
                                     appConfigPath + "/user-dirs.dirs"));

        for (const auto &pair : mountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };

            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
        }

        QList<QPair<QString, QString>> roMountMap;
        roMountMap.push_back(
                qMakePair(util::getUserFile(".local/share/fonts"), appLocalDataPath + "/fonts"));

        roMountMap.push_back(
                qMakePair(util::getUserFile(".config/fontconfig"), appConfigPath + "/fontconfig"));

        // mount fonts
        roMountMap.push_back(qMakePair(util::getUserFile(".local/share/fonts"),
                                       QString("/run/host/appearance/user-fonts")));

        // mount fonts cache
        roMountMap.push_back(qMakePair(util::getUserFile(".cache/fontconfig"),
                                       QString("/run/host/appearance/user-fonts-cache")));

        // mount dde-api
        // TODO ：主题相关，后续dde是否写成标准? 或者 此相关应用（如欢迎）不使用玲珑格式。
        auto ddeApiPath = util::ensureUserDir({ ".cache", "deepin", "dde-api" });
        roMountMap.push_back(qMakePair(ddeApiPath, ddeApiPath));
        roMountMap.push_back(qMakePair(ddeApiPath, appCachePath + "/deepin/dde-api"));

        // mount ~/.config/dconf
        // TODO: 所有应用主题相关设置数据保存在~/.config/dconf/user
        // 中，是否安全？一个应用沙箱中可以读取其他应用设置数据？ issues:
        // https://gitlabwh.uniontech.com/wuhan/v23/linglong/linglong/-/issues/72
        auto dconfPath = util::ensureUserDir({ ".config", "dconf" });
        roMountMap.push_back(qMakePair(dconfPath, appConfigPath + "/dconf"));

        QString xauthority = getenv("XAUTHORITY");
        roMountMap.push_back(qMakePair(xauthority, xauthority));

        for (const auto &pair : roMountMap) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "ro", "rbind" };
            m->source = pair.first;
            m->destination = pair.second;
            r->mounts.push_back(m);
            qDebug() << "mount app" << m->source << m->destination;
        }

        // 处理环境变量
        for (auto key : envMap.keys()) {
            if (linglong::util::envList.contains(key)) {
                r->process->env.push_back(key + "=" + envMap[key]);
            }
        }
        auto appRef = package::Ref(q_ptr->package->ref);
        auto appBinaryPath = QStringList{ "/opt/apps", appRef.appId, "files/bin" }.join("/");
        if (useFlatpakRuntime) {
            appBinaryPath = "/app/bin";
        }

        // 特殊处理env PATH
        if (envMap.contains("PATH")) {
            r->process->env.removeAt(r->process->env.indexOf(QRegExp("^PATH=.*"), 0));
            r->process->env.push_back("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":"
                                      + envMap["PATH"]);
        } else {
            r->process->env.push_back("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":"
                                      + getenv("PATH"));
        }

        // 特殊处理env HOME
        if (!envMap.contains("HOME")) {
            r->process->env.push_back("HOME=" + util::getUserFile(""));
        }

        r->process->env.push_back("XDG_RUNTIME_DIR=" + userRuntimeDir);
        r->process->env.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path="
                                  + util::jonsPath({ userRuntimeDir, "bus" }));

        auto appSharePath = QStringList{ "/opt/apps", appRef.appId, "files/share" }.join("/");
        if (useFlatpakRuntime) {
            appSharePath = "/app/share";
        }
        auto xdgDataDirs = QStringList{ appSharePath, "/runtime/share" };
        xdgDataDirs.append(qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share"));
        r->process->env.push_back("XDG_DATA_DIRS=" + xdgDataDirs.join(":"));

        // add env XDG_CONFIG_HOME XDG_CACHE_HOME
        // set env XDG_CONFIG_HOME=$(HOME)/.linglong/$(appId)/config
        r->process->env.push_back("XDG_CONFIG_HOME=" + appConfigPath);
        // set env XDG_CACHE_HOME=$(HOME)/.linglong/$(appId)/cache
        r->process->env.push_back("XDG_CACHE_HOME=" + appCachePath);

        // set env XDG_DATA_HOME=$(HOME)/.linglong/$(appId)/share
        r->process->env.push_back("XDG_DATA_HOME=" + appLocalDataPath);

        qDebug() << r->process->env;
        r->process->cwd = util::getUserFile("");

        QList<QList<quint64>> uidMaps = {
            { getuid(), 0, 1 },
        };
        for (auto const &uidMap : uidMaps) {
            Q_ASSERT(uidMap.size() == 3);
            QSharedPointer<IdMap> idMap(new IdMap);
            idMap->hostId = uidMap.value(0);
            idMap->containerId = uidMap.value(1);
            idMap->size = uidMap.value(2);
            r->linux->uidMappings.push_back(idMap);
        }

        QList<QList<quint64>> gidMaps = {
            { getgid(), 0, 1 },
        };
        for (auto const &gidMap : gidMaps) {
            Q_ASSERT(gidMap.size() == 3);
            QSharedPointer<IdMap> idMap(new IdMap);
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

        bool hasMountTmp = false;

        if (q->permissions && !q->permissions->mounts.isEmpty()) {
            // static mount
            for (const auto &mount : q->permissions->mounts) {
                QSharedPointer<Mount> m(new Mount);

                // illegal mount rules
                if (mount->source.isEmpty() || mount->destination.isEmpty()) {
                    m->deleteLater();
                    continue;
                }
                // fix default type
                if (mount->type.isEmpty()) {
                    m->type = "bind";
                } else {
                    m->type = mount->type;
                }

                // fix default options
                if (mount->options.isEmpty()) {
                    m->options = QStringList({ "ro", "rbind" });
                } else {
                    m->options = mount->options.split(",");
                }

                m->source = mount->source;
                m->destination = mount->destination;
                r->mounts.push_back(m);

                if (m->destination == "/tmp") {
                    hasMountTmp = true;
                }

                qDebug() << "add static mount:" << mount->source << " => " << mount->destination;
            }
        }

        if ((!hasMountTmp) && mountTmp()) {
            qWarning() << "fail to generate /tmp mount";
        }

        return 0;
    }

    int mountTmp()
    {
        const auto &tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

        // /tmp/linglong/{containerId}
        QDir tmp(QDir::cleanPath(tmpPath + QDir::separator() + "linglong" + QDir::separator()
                                 + this->container->id));
        if (!tmp.exists()) {
            if (!tmp.mkpath(".")) {
                return -1;
            }
        }
        QSharedPointer<Mount> m(new Mount);
        m->type = "bind";
        m->source = tmp.absolutePath();
        m->destination = tmpPath;
        m->options = QStringList({ "rbind" });
        r->mounts.push_back(m);
        return 0;
    }

    int fixMount(QString runtimeRootPath, const QString &appId)
    {
        // 360浏览器需要/apps-data/private/com.360.browser-stable目录可写
        // todo:后续360整改
        // 参考：https://gitlabwh.uniontech.com/wuhan/se/deepin-specifications/-/blob/master/unstable/%E5%BA%94%E7%94%A8%E6%95%B0%E6%8D%AE%E7%9B%AE%E5%BD%95%E8%A7%84%E8%8C%83.md

        if (QString("com.360.browser-stable") == appId) {
            // FIXME: 需要一个所有用户都有可读可写权限的目录
            QString appDataPath = util::getUserFile(".linglong/" + appId + "/share/appdata");
            linglong::util::ensureDir(appDataPath);
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rw", "rbind" };
            m->source = appDataPath;
            m->destination = "/apps-data/private/com.360.browser-stable";
            r->mounts.push_back(m);
        }

        // 挂载U盘目录
        auto uDiskDir = QStringList{ "/media", "/mnt" };
        for (auto dir : uDiskDir) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rw", "rbind" };
            m->source = dir;
            m->destination = dir;
            r->mounts.push_back(m);
        }

        // 挂载runtime的xdg-open和xdg-email到沙箱/usr/bin下
        auto xdgFileDirList = QStringList{ "xdg-open", "xdg-email" };
        for (auto dir : xdgFileDirList) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = runtimeRootPath + "/bin/" + dir;
            m->destination = "/usr/bin/" + dir;
            r->mounts.push_back(m);
        }

        // 存在 gschemas.compiled,需要挂载进沙箱
        if (linglong::util::fileExists(sysLinglongInstalltions
                                       + "/glib-2.0/schemas/gschemas.compiled")) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = sysLinglongInstalltions + "/glib-2.0/schemas/gschemas.compiled";
            m->destination = sysLinglongInstalltions + "/glib-2.0/schemas/gschemas.compiled";
            r->mounts.push_back(m);
        }

        // deepin-kwin share data with /tmp/screen-recorder
        {
            auto deepinKWinTmpSharePath = "/tmp/screen-recorder";
            util::ensureDir(deepinKWinTmpSharePath);
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = deepinKWinTmpSharePath;
            m->destination = deepinKWinTmpSharePath;
            r->mounts.push_back(m);
        }

        // deepin-mail need save data with /tmp/deepin-mail-web
        {
            auto deepinEmailTmpSharePath = "/tmp/deepin-mail-web";
            util::ensureDir(deepinEmailTmpSharePath);
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "rbind" };
            m->source = deepinEmailTmpSharePath;
            m->destination = deepinEmailTmpSharePath;
            r->mounts.push_back(m);
        }

        // Fixme: temporarily mount linglong root path into the container, remove later
        if (QString("org.deepin.manual") == appId) {
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "ro", "rbind" };
            m->source = util::getLinglongRootPath();
            m->destination = util::getLinglongRootPath();
            r->mounts.push_back(m);
        }

        // Fixme: temporarily mount linglong root path into the container, remove later
        // depends commit 248b39c8930deacea8f4e89b7ffedeee48fd8e0f
        if (QString("org.deepin.browser") == appId) {
            auto downloaderPath =
                    util::getLinglongRootPath() + "/" + "layers/org.deepin.downloader";
            QSharedPointer<Mount> m(new Mount);
            m->type = "bind";
            m->options = QStringList{ "ro", "rbind" };
            m->source = downloaderPath;
            m->destination = downloaderPath;
            r->mounts.push_back(m);
        }

        // mount /dev for app
        {
            QFile jsonFile(":/app_config.json");
            if (!jsonFile.open(QIODevice::ReadOnly)) {
                qCritical() << jsonFile.error() << jsonFile.errorString();
                return false;
            }
            auto json = QJsonDocument::fromJson(jsonFile.readAll());
            jsonFile.close();
            appConfig = json.toVariant().value<QSharedPointer<linglong::runtime::AppConfig>>();
            appConfig->setParent(q_ptr);
            for (const auto &appOfId : appConfig->appMountDevList) {
                if (appOfId == appId) {
                    QSharedPointer<Mount> m(new Mount);
                    m->type = "bind";
                    m->options = QStringList{ "rbind" };
                    m->source = "/dev";
                    m->destination = "/dev";
                    r->mounts.push_back(m);
                    break;
                }
            }
        }

        return 0;
    }

    static QString getMathedRuntime(const QString &runtimeId, const QString &runtimeVersion)
    {
        auto localRepoRoot = util::getLinglongRootPath() + "/layers/" + runtimeId;
        QDir appRoot(localRepoRoot);
        appRoot.setSorting(QDir::Name | QDir::Reversed);
        auto versionDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        auto available = linglong::util::APP_MIN_VERSION;
        for (const auto &item : versionDirs) {
            linglong::util::AppVersion versionIter(item);
            linglong::util::AppVersion dstVersion(available);
            if (versionIter.isValid() && versionIter.isBigThan(dstVersion)
                && item.startsWith(runtimeVersion)) {
                available = item;
            }
        }
        qDebug() << "getMathedRuntime info" << available << appRoot << versionDirs;
        return available;
    }

    // FIXME: none static
    static QString loadConfig(linglong::repo::Repo *repo,
                              const QString &appId,
                              const QString &appVersion,
                              const QString &channel,
                              const QString &module)
    {
        util::ensureUserDir({ ".linglong", appId });

        auto configPath =
                linglong::util::getUserFile(QString("%1/%2/app.yaml").arg(".linglong", appId));

        // create yaml form info
        // auto appRoot = LocalRepo::get()->rootOfLatest();
        auto latestAppRef = repo->latestOfRef(appId, appVersion);
        qDebug() << "loadConfig ref:" << latestAppRef.toLocalRefString();
        auto appInstallRoot = repo->rootOfLayer(latestAppRef);

        auto appInfo = appInstallRoot + "/info.json";
        // 判断是否存在
        if (!linglong::util::fileExists(appInfo)) {
            qCritical() << appInfo << " not exist";
            return QString();
        }

        // create a yaml config from json
        QSharedPointer<package::Info> info(util::loadJson<package::Info>(appInfo));

        if (info->runtime.isEmpty()) {
            // FIXME: return error is not exist

            // thin runtime
            info->runtime = "org.deepin.Runtime/20/x86_64";

            // full runtime
            // info->runtime = "deepin.Runtime.Sdk/23/x86_64";
        }

        package::Ref runtimeRef(info->runtime);
        QString appRef = QString("%1/").arg(channel)
                + latestAppRef.toLocalRefString().append(QString("/%1").arg(module));
        qDebug() << "loadConfig runtime" << info->runtime;
        // 获取最新版本runtime
        if (runtimeRef.version.split(".").size() != 4) {
            runtimeRef.version = getMathedRuntime(runtimeRef.appId, runtimeRef.version);
        }
        QString runtimeFullRef = QString("%1/").arg(channel)
                + runtimeRef.toLocalRefString().append(QString("/%1").arg(module));
        QMap<QString, QString> variables = {
            { "APP_REF", appRef },
            { "RUNTIME_REF", runtimeFullRef },
        };

        // TODO: remove to util module as file_template.cpp

        // permission load
        QMap<QString, QString> permissionMountsMap;

        QSharedPointer<package::User> permissionUserMounts;

        // old info.json load permission failed
        permissionUserMounts = info->permissions && info->permissions->filesystem
                        && info->permissions->filesystem->user
                ? info->permissions->filesystem->user
                : nullptr;

        if (permissionUserMounts != nullptr) {
            auto loadPermissionMap = QVariant(permissionUserMounts).toMap();
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
        for (const auto &k : variables.keys()) {
            templateData.replace(QString("@%1@").arg(k).toLocal8Bit(),
                                 variables.value(k).toLocal8Bit());
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
                        permissionMountsData += QString("\n    - type: bind\n      options: %1\n   "
                                                        "   source: %2\n      destination: %3")
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

    QSharedPointer<Container> container = nullptr;
    QSharedPointer<Runtime> r = nullptr;
    App *q_ptr = nullptr;
    QSharedPointer<AppConfig> appConfig = nullptr;

    repo::Repo *repo;
    int sockets[2]; // save file describers of sockets used to communicate with ll-box

    const QString sysLinglongInstalltions = util::getLinglongRootPath() + "/entries/share";

    Q_DECLARE_PUBLIC(App);
};

App::App(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new AppPrivate(this))
{
}

QSharedPointer<App> App::load(linglong::repo::Repo *repo,
                              const package::Ref &ref,
                              const QString &desktopExec)
{
    QString configPath =
            AppPrivate::loadConfig(repo, ref.appId, ref.version, ref.channel, ref.module);
    if (!linglong::util::fileExists(configPath)) {
        return nullptr;
    }

    QFile appConfig(configPath);
    appConfig.open(QIODevice::ReadOnly);

    qDebug() << "load conf yaml from" << configPath;

    QSharedPointer<App> app = nullptr;
    try {
        auto data = QString::fromLocal8Bit(appConfig.readAll());
        qDebug() << data;
        YAML::Node doc = YAML::Load(data.toStdString());
        app = QVariant::fromValue(doc).value<QSharedPointer<App>>();

        qDebug() << app << app->runtime << app->package << app->version;
        // TODO: maybe set as an arg of init is better
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

    // FIXME(black_desk): handle error
    auto data = (std::get<0>(util::toJSON<QSharedPointer<Runtime>>(d->r))).toStdString();

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, d->sockets) != 0) {
        return EXIT_FAILURE;
    }

    pid_t parent = getpid();

    pid_t boxPid = fork();
    if (boxPid < 0) {
        return -1;
    }

    if (0 == boxPid) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent) {
            raise(SIGKILL);
        }
        // child process
        (void)close(d->sockets[1]);
        auto socket = std::to_string(d->sockets[0]);
        char const *const args[] = { "ll-box", socket.c_str(), NULL };
        int ret = execvp(args[0], (char **)args);
        exit(ret);
    } else {
        close(d->sockets[0]);
        // FIXME: handle error
        (void)write(d->sockets[1], data.c_str(), data.size());
        (void)write(d->sockets[1], "\0", 1); // each data write into sockets should ended with '\0'
        d->container->pid = boxPid;
        // FIXME(interactive bash): if need keep interactive shell
        waitpid(boxPid, nullptr, 0);
        close(d->sockets[1]);
        // FIXME to do 删除代理socket临时文件
    }

    return EXIT_SUCCESS;
}

void App::exec(QString cmd, QString env, QString cwd)
{
    Q_D(App);

    QStringList envList = d->r->process->env;
    if (!env.isEmpty() && !env.isNull()) {
        envList = envList + env.split(",");
    }
    QSharedPointer<Process> p(new Process);
    if (cwd.isEmpty() || cwd.isNull()) {
        cwd = d->r->process->cwd;
    }
    p->cwd = cwd;
    p->env = envList;
    auto appCmd = util::splitExec(cmd);
    if (cmd.isEmpty() || cmd.isNull()) {
        // find desktop file
        auto appRef = package::Ref(package->ref);
        QString appRootPath = d->repo->rootOfLayer(appRef);

        QDir applicationsDir(
                QStringList{ appRootPath, "entries", "applications" }.join(QDir::separator()));
        auto desktopFilenameList = applicationsDir.entryList({ "*.desktop" }, QDir::Files);
        if (desktopFilenameList.length() <= 0) {
            return;
        }

        util::DesktopEntry desktopEntry(
                applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));

        // 当执行ll-cli run appid时，从entries目录获取执行参数
        auto execArgs = util::parseExec(desktopEntry.rawValue("Exec"));

        // 移除 ll-cli run xxx --exec参数
        auto execIndex = execArgs.indexOf("--exec");
        for (int i = 0; i <= execIndex; ++i) {
            execArgs.removeFirst();
        }
        // 移除类似%u/%F类型参数
        execArgs.removeAt(execArgs.indexOf(QRegExp("^%\\w$")));
        appCmd = execArgs;
    }

    if (appCmd.isEmpty()) {
        return;
    }
    p->args = appCmd;
    // FIXME(black_desk): handle error
    auto data = std::get<0>(util::toJSON(p)).toStdString();

    // FIXME: retry on temporary fail
    // FIXME: add lock
    int sizeOfData = data.size();
    while (sizeOfData) {
        auto sizeOfWrite = write(d->sockets[1], data.c_str(), sizeOfData);
        sizeOfData = sizeOfData - sizeOfWrite;
        data = data.erase(0, sizeOfWrite);
    }
    write(d->sockets[1], "\0", 1);
}

QSharedPointer<const Container> App::container() const
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

void App::setAppParamMap(const ParamStringMap &paramMap)
{
    Q_D(App);
    d->runParamMap = paramMap;
}

App::~App() = default;

} // namespace runtime
} // namespace linglong
