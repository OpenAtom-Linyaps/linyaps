/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app.h"

#include "linglong/dbus_ipc/package_manager_param.h"
#include "linglong/package/info.h"
#include "linglong/repo/repo.h"
#include "linglong/runtime/app_config.h"
#include "linglong/util/env.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/version/version.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/std_helper/qdebug_helper.h"
#include "linglong/utils/xdg/desktop_entry.h"
#include "ocppi/runtime/config/ConfigLoader.hpp"

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include <mutex>
#include <sstream>

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

static void initQResource()
{
    Q_INIT_RESOURCE(app_configs);
}

namespace linglong::runtime {

QSERIALIZER_IMPL(App);
QSERIALIZER_IMPL(AppPermission);
QSERIALIZER_IMPL(Layer);
QSERIALIZER_IMPL(MountYaml);

namespace PrivateAppInit {
auto init() -> int
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

auto App::init() -> bool
{
    QFile builtinOCIConfigTemplateJSONFile(":/config.json");
    if (!builtinOCIConfigTemplateJSONFile.open(QIODevice::ReadOnly)) {
        // NOTE(black_desk): Go check qrc if this occurs.
        qFatal("builtin OCI configuration template file missing.");
    }

    auto bytes = builtinOCIConfigTemplateJSONFile.readAll();
    std::stringstream tmpStream;
    tmpStream << bytes.toStdString();

    ocppi::runtime::config::ConfigLoader loader;
    auto config = loader.load(tmpStream);
    if (!config.has_value()) {
        qCritical() << "builtin OCI configuration template is invalid.";
        try {
            std::rethrow_exception(config.error());
        } catch (const std::exception &e) {
            qFatal("%s", e.what());
        } catch (...) {
            qFatal("Unknown error");
        }
    }

    r = std::move(config.value());

    container.reset(new Container(this));
    container->create(package->ref);

    return true;
}

int App::prepare()
{
    // FIXME: get info from module/package
    auto runtimeRef = package::Ref(runtime->ref);
    QString runtimeRootPath = repo->rootOfLayer(runtimeRef);

    // FIXME: return error if files not exist
    auto fixRuntimePath = runtimeRootPath + "/files";
    if (!util::dirExists(fixRuntimePath)) {
        fixRuntimePath = runtimeRootPath;
    }

    auto appRef = package::Ref(package->ref);
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
    for (const auto &env : *r.process->env) {
        envFile.write(env.c_str());
        envFile.write("\n");
    }
    envFile.close();

    ocppi::runtime::config::types::Mount m;
    m.type = "bind";
    m.options = { "rbind" };
    m.source = envFilepath.toStdString();
    m.destination = "/run/app/env";
    r.mounts->push_back(m);

    // TODO: move to class package
    // find desktop file
    QDir applicationsDir(
      QStringList{ appRootPath, "entries", "applications" }.join(QDir::separator()));
    auto desktopFilenameList = applicationsDir.entryList({ "*.desktop" }, QDir::Files);
    if (desktopFilenameList.length() <= 0) {
        return -1;
    }

    const auto &desktopEntry =
      utils::xdg::DesktopEntry::New(applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));
    if (!desktopEntry.has_value()) {
        // FIXME(black_desk): return error instead of logging here.
        qCritical() << "DesktopEntry file path:"
                    << applicationsDir.absoluteFilePath(desktopFilenameList.value(0));
        qCritical().noquote() << desktopEntry.error()->code() << desktopEntry.error()->message();
        return -1;
    }

    const auto &exec = desktopEntry->getValue<QString>("Exec");
    if (!exec.has_value()) {
        // FIXME(black_desk): return error instead of logging here.
        qCritical() << "Broken desktop file without Exec in main section.";
        qCritical().noquote() << exec.error()->code() << exec.error()->message();
        return -1;
    }

    const auto &parsedExec = util::parseExec(*exec);

    // FIXME(black_desk): remove this logic after upgarding old packages.
    // 当执行ll-cli run appid时，从entries目录获取执行参数，同时兼容旧的outputs打包模式。
    QStringList tmpArgs;
    QStringList execArgs;
    if (util::dirExists(QStringList{ appRootPath, "outputs", "share" }.join(QDir::separator()))) {
        execArgs = parsedExec;
    } else {
        tmpArgs = parsedExec;
        // 移除 ll-cli run  appid --exec 参数
        for (auto i = tmpArgs.indexOf(QRegExp("^--exec$")) + 1; i < tmpArgs.length(); ++i) {
            execArgs << tmpArgs[i];
        }
    }

    // FIXME(black_desk): ignore field codes here. This might cause bugs that:
    // - application has wrong icon to dispaly -> implement %i
    // - application has wrong translated name -> implement %c
    // - application doesn't know where its desktop entry placed -> implement %k
    for (auto index = execArgs.indexOf(QRegExp("^%\\w$")); index != -1;
         index = execArgs.indexOf(QRegExp("^%\\w$"))) {
        execArgs.removeAt(index);
    }

    if (r.process->args->empty()) {
        if (!desktopExec.isEmpty()) {
            execArgs = util::splitExec(desktopExec);
        }
        auto args = std::vector<std::string>{};
        for (const auto &arg : execArgs) {
            args.push_back(arg.toStdString());
        }
        r.process->args = args;
    }

    qDebug() << "exec" << *r.process->args;

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
    qInfo() << "createProxySocket path:" << sessionSocketPath << ", noDbusProxy:" << noDbusProxy;
    stateDBusProxyArgs(!noDbusProxy, appRef.appId, sessionSocketPath);
    return 0;
}

auto App::stageSystem() -> int
{
    QList<QPair<QString, QString>> mountMap;
    mountMap = {
        { "/dev/dri", "/dev/dri" },
        { "/dev/snd", "/dev/snd" },
    };

    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        qDebug() << "mount stageSystem" << m.source->c_str() << m.destination.c_str();
        r.mounts->push_back(std::move(m));
    }
    return 0;
}

auto App::stageRootfs(QString runtimeRootPath, const QString &appId, QString appRootPath) -> int
{
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

    r.annotations = {
        { "containerRootPath", container->workingDirectory.toStdString() },
    };

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
            path.replace(QString("$%1").arg(key).toLocal8Bit(), variables.value(key).toLocal8Bit());
        }
        return path;
    };

    // 通过runtime info.json文件获取basicsRootPath路径
    auto runtimeRef = package::Ref(runtime->ref);
    QString runtimePath = repo->rootOfLayer(runtimeRef);
    auto runtimeInfoFile = runtimePath + "/info.json";
    // basics usr
    QString basicsUsrRootPath = "";
    // basics etc
    QString basicsEtcRootPath = "";
    if (!linglong::util::isDeepinSysProduct()) {
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
    auto debugRef = package::Ref(package->ref);
    qDebug() << "stageRootfs debugRef " << package->ref;
    if ("devel" == debugRef.module) {
        fuseMount = true;
    }

    if (fuseMount) {
        r.annotations->insert({
          "overlayfs",
          {
            { "lowerParent",
              QStringList{ container->workingDirectory, ".overlayfs", "lower_parent" }
                .join("/")
                .toStdString() },
            { "upper",
              QStringList{ container->workingDirectory, ".overlayfs", "upper" }
                .join("/")
                .toStdString() },
            { "workdir",
              QStringList{ container->workingDirectory, ".overlayfs", "workdir" }
                .join("/")
                .toStdString() },
          },
        });
    } else {
        r.annotations->insert({ "native", {} });
    }

    r.annotations->insert({ "dbusProxyInfo", {} });

    QList<QPair<QString, QString>> mountMap;

    mountMap = {
        { "/usr", "/usr" },
        { "/etc", "/etc" },
        { runtimeRootPath, "/runtime" },
        { "/usr/share/locale/", "/usr/share/locale/" },
    };

    qDebug() << "stageRootfs runtimeRootPath:" << runtimeRootPath << "appRootPath:" << appRootPath;
    // appRootPath/devel/files/debug /usr/lib/debug/opt/apps/appid/files 挂载调试符号
    if ("devel" == debugRef.module) {
        mountMap.push_back({ appRootPath + "/devel/files/debug",
                             "/usr/lib/debug/opt/apps/" + debugRef.appId + "/files" });
        // runtime 只用挂载devel/files/debug 目录
        mountMap.push_back(
          { runtimeRootPath.left(runtimeRootPath.length() - QString("/files").length())
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
        mountMap.push_back(
          { runtimeRootPath + "/opt/deepin-wine6-stable", "/opt/deepin-wine6-stable" });
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

    for (const auto &pair : mountMap) {
        nlohmann::json m;
        m["type"] = "bind";
        m["options"] = nlohmann::json::array_t{ "ro", "rbind" };
        m["source"] = pair.first.toStdString();
        m["destination"] = pair.second.toStdString();

        if (fuseMount) {
            // overlay mount 顺序是反向的
            if (wineMount) {
                // wine应用先保持不变（会导致wine应用运行失败），后续整改
                (*r.annotations)["overlayfs"]["mounts"].push_back(m);
            } else {
                // ll-box overlay失效，待修复后改为push_front（gnome应用运行失效）
                (*r.annotations)["overlayfs"]["mounts"].push_back(m);
            }
        } else {
            (*r.annotations)["native"]["mounts"].push_back(m);
        }
    }

    // 读写挂载/opt,有的应用需要读写自身携带的资源文件。eg:云看盘
    QString appMountPath = "";
    appMountPath = "/opt/apps/" + appId;

    nlohmann::json m;
    m["type"] = "bind";
    m["options"] = nlohmann::json::array_t{ "rw", "rbind" };
    m["source"] = appRootPath.toStdString();
    m["destination"] = appMountPath.toStdString();

    if (fuseMount) {
        // overlay mount 顺序是反向的
        if (wineMount) {
            // wine应用先保持不变（会导致wine应用运行失败），后续整改
            (*r.annotations)["overlayfs"]["mounts"].push_back(m);
        } else {
            // ll-box overlay失效，待修复后改为push_front（gnome应用运行失效）
            (*r.annotations)["overlayfs"]["mounts"].push_back(m);
        }
    } else {
        (*r.annotations)["native"]["mounts"].push_back(m);
    }

    // TODO(iceyer): let application do this or add to doc
    auto appLdLibraryPath = QStringList{ "/opt/apps", appId, "files/lib" }.join("/");

    // todo: 代码冗余，后续整改，配置文件？
    QStringList fixLdLibraryPath;
    RunArch runArch;
    auto appRef = package::Ref(package->ref);
    if (appRef.arch == "arm64") {
        runArch = ARM64;
    } else if (appRef.arch == "x86_64") {
        runArch = X86_64;
    } else {
        runArch = UNKNOWN;
    }

    r.process->env = r.process->env.value_or(std::vector<std::string>{});
    auto &env = r.process->env.value();

    switch (runArch) {
    case ARM64:
        fixLdLibraryPath = QStringList{
            appLdLibraryPath, appLdLibraryPath + "/aarch64-linux-gnu",
            "/runtime/lib",   "/runtime/lib/aarch64-linux-gnu",
            "/usr/lib",       "/usr/lib/aarch64-linux-gnu",
        };
        env.push_back(("QT_PLUGIN_PATH=/opt/apps/" + appId
                       + "/files/plugins:/runtime/lib/aarch64-linux-gnu/qt5/"
                         "plugins:/usr/lib/aarch64-linux-gnu/qt5/plugins")
                        .toStdString());
        env.push_back(("QT_QPA_PLATFORM_PLUGIN_PATH=/opt/apps/" + appId
                       + "/files/plugins/platforms:/runtime/lib/aarch64-linux-gnu/qt5/plugins/"
                         "platforms:/usr/lib/aarch64-linux-gnu/qt5/plugins/platforms")
                        .toStdString());
        if (!fuseMount) {
            env.push_back(
              ("GST_PLUGIN_PATH=/opt/apps/" + appId + "/files/lib/aarch64-linux-gnu/gstreamer-1.0")
                .toStdString());
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
        env.push_back(("QT_PLUGIN_PATH=/opt/apps/" + appId
                       + "/files/plugins:/runtime/lib/x86_64-linux-gnu/qt5/plugins:/"
                         "usr/lib/x86_64-linux-gnu/qt5/plugins")
                        .toStdString());
        env.push_back(("QT_QPA_PLATFORM_PLUGIN_PATH=/opt/apps/" + appId
                       + "/files/plugins/platforms:/runtime/lib/x86_64-linux-gnu/qt5/plugins/"
                         "platforms:/usr/lib/x86_64-linux-gnu/qt5/plugins/platforms")
                        .toStdString());
        if (!fuseMount) {
            env.push_back(
              ("GST_PLUGIN_PATH=/opt/apps/" + appId + "/files/lib/x86_64-linux-gnu/gstreamer-1.0")
                .toStdString());
        }
        break;
    default:
        qInfo() << "no supported arch :" << appRef.arch;
        return -1;
    }

    env.push_back(("LD_LIBRARY_PATH=" + fixLdLibraryPath.join(":")).toStdString());
    return 0;
}

auto App::stageHost() -> int
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
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = std::vector<std::string>{ "ro", "rbind" };
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
    }

    QList<QPair<QString, QString>> mountMap = {
        { "/tmp/.X11-unix", "/tmp/.X11-unix" }, // FIXME: only mount one DISPLAY
    };

    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
    }

    return 0;
}

void App::stateDBusProxyArgs(bool enable, const QString &appId, const QString &proxyPath)
{
    auto &anno = *r.annotations;
    anno["dbusProxyInfo"]["appId"] = appId.toStdString();
    anno["dbusProxyInfo"]["enable"] = enable;
    if (!enable) {
        return;
    }
    anno["dbusProxyInfo"]["busType"] = runParamMap[linglong::util::kKeyBusType].toStdString();
    anno["dbusProxyInfo"]["proxyPath"] = proxyPath.toStdString();
    // FIX to do load filter from yaml
    // FIX to do 加载用户配置参数（权限管限器上）
    // 添加cli command运行参数
    if (runParamMap.contains(linglong::util::kKeyFilterName)) {
        QString name = runParamMap[linglong::util::kKeyFilterName];
        auto &names = anno["dbusProxyInfo"]["name"] = nlohmann::json::array_t{};
        names.push_back(name.toStdString());
    }
    if (runParamMap.contains(linglong::util::kKeyFilterPath)) {
        QString path = runParamMap[linglong::util::kKeyFilterPath];
        auto &paths = anno["dbusProxyInfo"]["path"] = nlohmann::json::array_t{};
        paths.push_back(path.toStdString());
    }
    if (runParamMap.contains(linglong::util::kKeyFilterIface)) {
        QString interface = runParamMap[linglong::util::kKeyFilterIface];
        auto &interfaces = anno["dbusProxyInfo"]["interface"] = nlohmann::json::array_t{};
        interfaces.push_back(interface.toStdString());
    }
}

// Fix to do 当前仅处理session bus
auto App::stageDBusProxy(const QString &socketPath, bool useDBusProxy) -> int
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
        ocppi::runtime::config::types::Mount m;

        m.type = "bind";
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
    }

    return 0;
}

auto App::stageUser(const QString &appId) -> int
{
    QList<QPair<QString, QString>> mountMap;

    // bind user data
    auto userRuntimeDir = QString("/run/user/%1").arg(getuid());
    {
        ocppi::runtime::config::types::Mount m;
        m.type = "tmpfs";
        m.options = { "nodev", "nosuid", "mode=700" };
        m.source = "tmpfs";
        m.destination = userRuntimeDir.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
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
    auto appUserSystemdPath = util::ensureUserDir({ ".linglong", appId, "/config/systemd/user" });
    mountMap.push_back(qMakePair(util::getUserFile(".config/systemd/user"), appUserSystemdPath));

    // mount .config/user-dirs.dirs todo:移除挂载到~/.config下？
    mountMap.push_back(qMakePair(util::getUserFile(".config/user-dirs.dirs"),
                                 util::getUserFile(".config/user-dirs.dirs")));
    mountMap.push_back(
      qMakePair(util::getUserFile(".config/user-dirs.dirs"), appConfigPath + "/user-dirs.dirs"));

    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };

        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
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
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "ro", "rbind" };
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r.mounts->push_back(m);
        qDebug() << "mount app" << m.source->c_str() << m.destination.c_str();
    }

    // 处理环境变量
    for (auto key : envMap.keys()) {
        if (linglong::util::envList.contains(key)) {
            r.process->env->push_back((key + "=" + envMap[key]).toStdString());
        }
    }
    auto appRef = package::Ref(package->ref);
    auto appBinaryPath = QStringList{ "/opt/apps", appRef.appId, "files/bin" }.join("/");

    // 特殊处理env PATH
    r.process->env = r.process->env.value_or(std::vector<std::string>{});
    auto &env = r.process->env.value();
    auto newEnv = std::remove_reference_t<decltype(env)>{};

    for (auto &envEntry : env) {
        if (!(envEntry.rfind("PATH=", 0) == 0)) {
            newEnv.push_back(envEntry);
        }
    }

    env = std::move(newEnv);

    if (envMap.contains("PATH")) {
        env.push_back(
          ("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":" + envMap["PATH"]).toStdString());
    } else {
        env.push_back(
          ("PATH=" + appBinaryPath + ":" + "/runtime/bin" + ":" + getenv("PATH")).toStdString());
    }

    // 特殊处理env HOME
    if (!envMap.contains("HOME")) {
        env.push_back(("HOME=" + util::getUserFile("")).toStdString());
    }

    env.push_back("XDG_RUNTIME_DIR=" + userRuntimeDir.toStdString());
    env.push_back("DBUS_SESSION_BUS_ADDRESS=unix:path="
                  + util::jonsPath({ userRuntimeDir, "bus" }).toStdString());

    auto appSharePath = QStringList{ "/opt/apps", appRef.appId, "files/share" }.join("/");
    auto xdgDataDirs = QStringList{ appSharePath, "/runtime/share" };
    xdgDataDirs.append(qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share"));
    env.push_back("XDG_DATA_DIRS=" + xdgDataDirs.join(":").toStdString());

    // add env XDG_CONFIG_HOME XDG_CACHE_HOME
    // set env XDG_CONFIG_HOME=$(HOME)/.linglong/$(appId)/config
    env.push_back("XDG_CONFIG_HOME=" + appConfigPath.toStdString());
    // set env XDG_CACHE_HOME=$(HOME)/.linglong/$(appId)/cache
    env.push_back("XDG_CACHE_HOME=" + appCachePath.toStdString());

    // set env XDG_DATA_HOME=$(HOME)/.linglong/$(appId)/share
    env.push_back("XDG_DATA_HOME=" + appLocalDataPath.toStdString());

    qDebug() << env;
    r.process->cwd = util::getUserFile("").toStdString();

    QList<QList<int64_t>> uidMaps = {
        { getuid(), 0, 1 },
    };
    for (auto const &uidMap : uidMaps) {
        Q_ASSERT(uidMap.size() == 3);
        ocppi::runtime::config::types::IdMapping idMapping{};
        idMapping.hostID = uidMap.value(0);
        idMapping.containerID = uidMap.value(1);
        idMapping.size = uidMap.value(2);
        r.linux_->uidMappings->push_back(idMapping);
    }

    QList<QList<int64_t>> gidMaps = {
        { getgid(), 0, 1 },
    };
    for (auto const &gidMap : gidMaps) {
        ocppi::runtime::config::types::IdMapping idMapping{};
        idMapping.hostID = gidMap.value(0);
        idMapping.containerID = gidMap.value(1);
        idMapping.size = gidMap.value(2);
        r.linux_->gidMappings->push_back(idMapping);
    }

    return 0;
}

auto App::stageMount() -> int
{
    bool hasMountTmp = false;

    if (permissions && !permissions->mounts.isEmpty()) {
        // static mount
        for (const auto &mount : permissions->mounts) {
            ocppi::runtime::config::types::Mount m;

            // illegal mount rules
            if (mount->source.isEmpty() || mount->destination.isEmpty()) {
                continue;
            }
            // fix default type
            if (mount->type.isEmpty()) {
                m.type = "bind";
            } else {
                m.type = mount->type.toStdString();
            }

            // fix default options
            if (mount->options.isEmpty()) {
                m.options = { "ro", "rbind" };
            } else {
                m.options = {};
                for (auto &opt : mount->options.split(",")) {
                    m.options->push_back(opt.toStdString());
                }
            }

            m.source = mount->source.toStdString();
            m.destination = mount->destination.toStdString();
            r.mounts->push_back(m);

            if (m.destination == "/tmp") {
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

auto App::mountTmp() -> int
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
    ocppi::runtime::config::types::Mount m;
    m.type = "bind";
    m.source = tmp.absolutePath().toStdString();
    m.destination = tmpPath.toStdString();
    m.options = { "rbind" };
    r.mounts->push_back(m);
    return 0;
}

auto App::fixMount(QString runtimeRootPath, const QString &appId) -> int
{
    // 360浏览器需要/apps-data/private/com.360.browser-stable目录可写
    // todo:后续360整改
    // 参考：https://gitlabwh.uniontech.com/wuhan/se/deepin-specifications/-/blob/master/unstable/%E5%BA%94%E7%94%A8%E6%95%B0%E6%8D%AE%E7%9B%AE%E5%BD%95%E8%A7%84%E8%8C%83.md

    if (QString("com.360.browser-stable") == appId) {
        // FIXME: 需要一个所有用户都有可读可写权限的目录
        QString appDataPath = util::getUserFile(".linglong/" + appId + "/share/appdata");
        linglong::util::ensureDir(appDataPath);
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rw", "rbind" };
        m.source = appDataPath.toStdString();
        m.destination = "/apps-data/private/com.360.browser-stable";
        r.mounts->push_back(m);
    }

    // 挂载U盘目录
    auto uDiskDir = QStringList{ "/media", "/mnt" };
    for (auto dir : uDiskDir) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rw", "rbind" };
        m.source = dir.toStdString();
        m.destination = dir.toStdString();
        r.mounts->push_back(m);
    }

    // 挂载runtime的xdg-open和xdg-email到沙箱/usr/bin下
    auto xdgFileDirList = QStringList{ "xdg-open", "xdg-email" };
    for (auto dir : xdgFileDirList) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = (runtimeRootPath + "/bin/" + dir).toStdString();
        m.destination = "/usr/bin/" + dir.toStdString();
        r.mounts->push_back(m);
    }

    // 存在 gschemas.compiled,需要挂载进沙箱
    if (linglong::util::fileExists(sysLinglongInstalltions
                                   + "/glib-2.0/schemas/gschemas.compiled")) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = sysLinglongInstalltions.toStdString() + "/glib-2.0/schemas/gschemas.compiled";
        m.destination =
          sysLinglongInstalltions.toStdString() + "/glib-2.0/schemas/gschemas.compiled";
        r.mounts->push_back(m);
    }

    // deepin-kwin share data with /tmp/screen-recorder
    {
        auto deepinKWinTmpSharePath = "/tmp/screen-recorder";
        util::ensureDir(deepinKWinTmpSharePath);
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = deepinKWinTmpSharePath;
        m.destination = deepinKWinTmpSharePath;
        r.mounts->push_back(m);
    }

    // deepin-mail need save data with /tmp/deepin-mail-web
    {
        auto deepinEmailTmpSharePath = "/tmp/deepin-mail-web";
        util::ensureDir(deepinEmailTmpSharePath);
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "rbind" };
        m.source = deepinEmailTmpSharePath;
        m.destination = deepinEmailTmpSharePath;
        r.mounts->push_back(m);
    }

    // Fixme: temporarily mount linglong root path into the container, remove later
    if (QString("org.deepin.manual") == appId) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "ro", "rbind" };
        m.source = util::getLinglongRootPath().toStdString();
        m.destination = util::getLinglongRootPath().toStdString();
        r.mounts->push_back(m);
    }

    // Fixme: temporarily mount linglong root path into the container, remove later
    // depends commit 248b39c8930deacea8f4e89b7ffedeee48fd8e0f
    if (QString("org.deepin.browser") == appId) {
        auto downloaderPath = util::getLinglongRootPath() + "/" + "layers/org.deepin.downloader";
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "ro", "rbind" };
        m.source = downloaderPath.toStdString();
        m.destination = downloaderPath.toStdString();
        r.mounts->push_back(m);
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
        appConfig->setParent(this);
        for (const auto &appOfId : appConfig->appMountDevList) {
            if (appOfId == appId) {
                ocppi::runtime::config::types::Mount m;
                m.type = "bind";
                m.options = { "rbind" };
                m.source = "/dev";
                m.destination = "/dev";
                r.mounts->push_back(m);
                break;
            }
        }
    }

    return 0;
}

auto App::getMathedRuntime(const QString &runtimeId, const QString &runtimeVersion) -> QString
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

auto App::loadConfig(linglong::repo::Repo *repo,
                     const QString &appId,
                     const QString &appVersion,
                     const QString &channel,
                     const QString &module) -> QString
{
    util::ensureUserDir({ ".linglong", appId });

    auto configPath =
      linglong::util::getUserFile(QString("%1/%2/app.yaml").arg(".linglong", appId));

    // create yaml form info
    // auto appRoot = LocalRepo::get()->rootOfLatest();
    auto latestAppRef = repo->latestOfRef(appId, appVersion);
    qDebug() << "loadConfig ref:" << latestAppRef.toSpecString();
    auto appInstallRoot = repo->rootOfLayer(latestAppRef);

    auto appInfo = appInstallRoot + "/info.json";
    // 判断是否存在
    if (!linglong::util::fileExists(appInfo)) {
        qCritical() << appInfo << " not exist";
        return {};
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
    permissionUserMounts =
      info->permissions && info->permissions->filesystem && info->permissions->filesystem->user
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

App::App(QObject *parent)
    : JsonSerialize(parent)
{
}

auto App::load(linglong::repo::Repo *repo, const package::Ref &ref, const QString &desktopExec)
  -> QSharedPointer<App>
{
    QString configPath = loadConfig(repo, ref.appId, ref.version, ref.channel, ref.module);
    if (!linglong::util::fileExists(configPath)) {
        return nullptr;
    }

    QFile appConfig(configPath);
    appConfig.open(QIODevice::ReadOnly);

    qDebug() << "load app config yaml from" << configPath;

    auto [app, err] = util::fromYAML<QSharedPointer<App>>(appConfig.readAll());
    if (err) {
        qCritical() << "FIXME: load config failed, use default app config";
    }

    qDebug() << "app config" << app << app->runtime << app->package << app->version;
    // TODO: maybe set as an arg of init is better
    app->desktopExec = desktopExec;
    app->repo = repo;
    app->init();

    return app;
}

auto App::start() -> util::Error
{
    r.root->path = container->workingDirectory.toStdString() + "/root";
    util::ensureDir(r.root->path.c_str());

    prepare();

    // write pid file
    QFile pidFile(container->workingDirectory + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << r.root->path.c_str();

    auto runtimeConfigJSON = toJSON(r);

    auto data = runtimeConfigJSON.dump();

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets) != 0) {
        return WrapError(NewError(errno, strerror(errno)), "call socketpair failed");
    }

    pid_t parent = getpid();

    pid_t boxPid = fork();
    if (boxPid < 0) {
        return WrapError(NewError(errno, strerror(errno)), "fork failed");
    }

    if (0 == boxPid) {
        // child process
        qDebug() << "start ll-box in child process";
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent) {
            raise(SIGKILL);
        }
        (void)close(sockets[1]);
        auto socket = std::to_string(sockets[0]);
        char const *const args[] = { "ll-box", socket.c_str(), nullptr };
        auto ret = execvp(args[0], (char **)args);
        exit(ret);
    } else {
        qDebug() << "wait child" << boxPid << "in parent";
        close(sockets[0]);
        // FIXME: handle error
        (void)write(sockets[1], data.c_str(), data.size());
        (void)write(sockets[1], "\0", 1); // each data write into sockets should end with '\0'
        container->pid = boxPid;
        // FIXME: need keep interactive shell
        auto pid = waitpid(boxPid, nullptr, 0);
        close(sockets[1]);
        // FIXME: 删除代理socket临时文件
        // FIXME: 清理资源，包括挂载的VFS等
        qDebug() << "child" << pid << "finish";
    }

    return Success();
}

void App::exec(QString cmd, QString env, QString cwd)
{
    ocppi::runtime::config::types::Process p;
    p.env = r.process->env;

    if (!env.isEmpty() && !env.isNull()) {
        for (const auto &env : env.split(",")) {
            p.env = p.env.value_or(std::vector<std::string>());
            p.env->push_back(env.toStdString());
        }
    }

    if (cwd.isEmpty() || cwd.isNull()) {
        cwd = r.process->cwd.c_str();
    }

    p.cwd = cwd.toStdString();

    auto appCmd = util::splitExec(cmd);
    if (cmd.isEmpty() || cmd.isNull()) {
        // find desktop file
        auto appRef = package::Ref(package->ref);
        QString appRootPath = repo->rootOfLayer(appRef);

        QDir applicationsDir(
          QStringList{ appRootPath, "entries", "applications" }.join(QDir::separator()));
        auto desktopFilenameList = applicationsDir.entryList({ "*.desktop" }, QDir::Files);
        if (desktopFilenameList.length() <= 0) {
            return;
        }

        const auto &desktopEntry = utils::xdg::DesktopEntry::New(
          applicationsDir.absoluteFilePath(desktopFilenameList.value(0)));
        if (!desktopEntry.has_value()) {
            // FIXME(black_desk): return error instead of logging here.
            qCritical() << "DesktopEntry file path:"
                        << applicationsDir.absoluteFilePath(desktopFilenameList.value(0));
            qCritical().noquote() << desktopEntry.error()->code()
                                  << desktopEntry.error()->message();
            return;
        }

        const auto &exec = desktopEntry->getValue<QString>("Exec");
        if (!exec.has_value()) {
            // FIXME(black_desk): return error instead of logging here.
            qCritical() << "Broken desktop file without Exec in main section.";
            qCritical().noquote() << exec.error()->code() << exec.error()->message();
            return;
        }

        const auto &parsedExec = util::parseExec(*exec);

        // 当执行ll-cli run appid时，从entries目录获取执行参数
        auto execArgs = parsedExec;

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

    auto args = std::vector<std::string>{};
    for (const auto &arg : appCmd) {
        args.push_back(arg.toStdString());
    }

    qDebug() << "exec" << *r.process->args;

    p.args = args;
    auto processJSON = toJSON(p);
    auto data = processJSON.dump();

    // FIXME: retry on temporary fail
    // FIXME: add lock
    int sizeOfData = data.size();
    while (sizeOfData) {
        auto sizeOfWrite = write(sockets[1], data.c_str(), sizeOfData);
        sizeOfData = sizeOfData - sizeOfWrite;
        data = data.erase(0, sizeOfWrite);
    }
    write(sockets[1], "\0", 1);
}

void App::saveUserEnvList(const QStringList &userEnvList)
{
    for (auto env : userEnvList) {
        auto sepPos = env.indexOf("=");
        envMap.insert(env.left(sepPos), env.right(env.length() - sepPos - 1));
    }
}

void App::setAppParamMap(const ParamStringMap &paramMap)
{
    runParamMap = paramMap;
}

App::~App() = default;

} // namespace linglong::runtime
