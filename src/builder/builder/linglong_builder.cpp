/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "linglong_builder.h"

#include <csignal>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QTemporaryFile>
#include <QThread>

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/xdg.h"
#include "module/runtime/oci.h"
#include "module/runtime/container.h"
#include "module/repo/ostree_repo.h"

#include "source_fetcher.h"
#include "builder_config.h"
#include "depend_fetcher.h"
#include "module/util/desktop_entry.h"
#include "module/util/sysinfo.h"
#include "module/runtime/app.h"
#include "cli/cmd/command_helper.h"
#include "module/util/env.h"

namespace linglong {
namespace builder {

linglong::util::Error commitBuildOutput(Project *project, AnnotationsOverlayfsRootfs *overlayfs)
{
    auto output = project->config().cacheInstallPath("files");
    linglong::util::ensureDir(output);

    auto upperDir = QStringList {overlayfs->upper, project->config().targetInstallPath("")}.join("/");
    linglong::util::ensureDir(upperDir);

    auto lowerDir = project->config().cacheAbsoluteFilePath({"overlayfs", "lower"});
    // if combine runtime, lower is ${PROJECT_CACHE}/runtime/files
    if (PackageKindRuntime == project->package->kind) {
        lowerDir = project->config().cacheAbsoluteFilePath({"runtime", "files"});
    }
    linglong::util::ensureDir(lowerDir);

    QProcess fuseOverlayfs;
    fuseOverlayfs.setProgram("fuse-overlayfs");
    fuseOverlayfs.setArguments({
        "-f",
        "-o",
        "auto_unmount",
        "-o",
        "upperdir=" + upperDir,
        "-o",
        "workdir=" + overlayfs->workdir,
        "-o",
        "lowerdir=" + lowerDir,
        output,
    });

    qint64 fuseOverlayfsPid = -1;
    fuseOverlayfs.startDetached(&fuseOverlayfsPid);
    fuseOverlayfs.waitForStarted(-1);

    // FIXME: must wait fuse mount filesystem
    QThread::sleep(1);

    auto entriesPath = project->config().cacheInstallPath("entries");

    auto modifyConfigFile = [](const QString &srcPath, const QString &targetPath, const QString &fileType,
                               const QString &appId) -> util::Error {
        QDir configDir(srcPath);
        auto configFileInfoList = configDir.entryInfoList({fileType}, QDir::Files);

        linglong::util::ensureDir(targetPath);

        for (auto const &fileInfo : configFileInfoList) {
            util::DesktopEntry desktopEntry(fileInfo.filePath());

            // set all section
            auto configSections = desktopEntry.sections();
            for (auto section : configSections) {
                auto exec = desktopEntry.rawValue("Exec", section);
                exec = QString("ll-cli run %1 --exec %2").arg(appId, exec);
                desktopEntry.set(section, "Exec", exec);

                // The section TryExec affects starting from the launcher, set it to null.
                auto tryExec = desktopEntry.rawValue("TryExec", section);

                if (!tryExec.isEmpty()) {
                    desktopEntry.set(section, "TryExec", "");
                }
                auto ret = desktopEntry.save(QStringList {targetPath, fileInfo.fileName()}.join(QDir::separator()));
                if (!ret.success()) {
                    return WrapError(ret, "save config failed");
                }
            }
        }
        return NoError();
    };

    auto appId = project->package->id;
    // modify desktop file
    auto desktopFilePath = QStringList {output, "share/applications"}.join(QDir::separator());
    auto desktopFileSavePath = QStringList {entriesPath, "applications"}.join(QDir::separator());
    auto modifyRet = modifyConfigFile(desktopFilePath, desktopFileSavePath, "*.desktop", appId);
    if (!modifyRet.success()) {
        kill(fuseOverlayfsPid, SIGTERM);
        return modifyRet;
    }

    // modify context-menus file
    auto contextFilePath = QStringList {output, "share/applications/context-menus"}.join(QDir::separator());
    auto contextFileSavePath = contextFilePath;
    modifyRet = modifyConfigFile(contextFilePath, contextFileSavePath, "*.conf", appId);
    if (!modifyRet.success()) {
        kill(fuseOverlayfsPid, SIGTERM);
        return modifyRet;
    }

    auto moveDir = [](const QStringList targetList, const QString &srcPath,
                      const QString &destPath) -> linglong::util::Error {
        for (auto target : targetList) {
            auto srcDir = QStringList {srcPath, target}.join(QDir::separator());
            auto destDir = QStringList {destPath, target}.join(QDir::separator());

            if (util::dirExists(srcDir)) {
                util::copyDir(srcDir, destDir);
                util::removeDir(srcDir);
            }
        }

        return NoError();
    };

    // Move some directories in files/share to entries
    // directories like icons, mime, dbus-1, locale, deepin-manual
    auto moveStatus = moveDir({"icons", "mime", "dbus-1", "locale", "autostart", "deepin-manual"},
                              project->config().cacheInstallPath("files/share"), entriesPath);
    if (!moveStatus.success()) {
        kill(fuseOverlayfsPid, SIGTERM);
        return moveStatus;
    }

    // Move files/lib/systemd to entries/systemd
    if (util::dirExists(project->config().cacheInstallPath("files/lib/systemd"))) {
        util::copyDir(project->config().cacheInstallPath("files/lib/systemd"),
                      QStringList {entriesPath, "systemd"}.join(QDir::separator()));

        util::removeDir(project->config().cacheInstallPath("files/lib/systemd"));
    }

    // Move runtime-install/files/debug to devel-install/files/debug
    linglong::util::ensureDir(project->config().cacheInstallPath("devel-install", "files"));
    moveStatus =
        moveDir({"debug", "include", "mkspec ", "cmake", "pkgconfig"}, project->config().cacheInstallPath("files"),
                project->config().cacheInstallPath("devel-install", "files"));
    if (!moveStatus.success()) {
        kill(fuseOverlayfsPid, SIGTERM);

        return moveStatus;
    }

    auto createInfo = [](Project *project) -> linglong::util::Error {
        package::Info info;

        info.kind = project->package->kind;
        info.version = project->package->version;

        info.base = project->baseRef().toLocalRefString();

        info.runtime = project->runtimeRef().toLocalRefString();

        info.appid = project->package->id;
        info.name = project->package->name;
        info.description = project->package->description;
        info.arch = QStringList {project->config().targetArch()};

        info.module = "runtime";
        info.size = linglong::util::sizeOfDir(project->config().cacheInstallPath(""));
        QFile infoFile(project->config().cacheInstallPath("info.json"));
        if (!infoFile.open(QIODevice::WriteOnly)) {
            return NewError(infoFile.error(), infoFile.errorString() + " " + infoFile.fileName());
        }
        if (infoFile.write(QJsonDocument::fromVariant(toVariant<package::Info>(&info)).toJson()) < 0) {
            return NewError(infoFile.error(), infoFile.errorString());
        }
        infoFile.close();

        info.module = "devel";
        info.size = linglong::util::sizeOfDir(project->config().cacheInstallPath("devel-install"));
        QFile develInfoFile(project->config().cacheInstallPath("devel-install", "info.json"));
        if (!develInfoFile.open(QIODevice::WriteOnly)) {
            return NewError(develInfoFile.error(), develInfoFile.errorString() + " " + develInfoFile.fileName());
        }
        if (develInfoFile.write(QJsonDocument::fromVariant(toVariant<package::Info>(&info)).toJson()) < 0) {
            return NewError(develInfoFile.error(), develInfoFile.errorString());
        }
        develInfoFile.close();

        QFile sourceConfigFile(project->configFilePath());
        if (!sourceConfigFile.copy(project->config().cacheInstallPath("linglong.yaml"))) {
            return NewError(sourceConfigFile.error(), sourceConfigFile.errorString());
        }

        return NoError();
    };

    auto ret = createInfo(project);
    if (!ret.success()) {
        kill(fuseOverlayfsPid, SIGTERM);
        return NewError(ret, -1, "createInfo failed");
    }

    repo::OSTreeRepo repo(BuilderConfig::instance()->repoPath());

    ret = repo.importDirectory(project->refWithModule("runtime"), project->config().cacheInstallPath(""));

    if (!ret.success()) {
        kill(fuseOverlayfsPid, SIGTERM);
        qCritical() << QString("commit %1 filed").arg(project->refWithModule("runtime").toString());
        return ret;
    }

    ret =
        repo.importDirectory(project->refWithModule("devel"), project->config().cacheInstallPath("devel-install", ""));

    //        fuseOverlayfs.kill();
    kill(fuseOverlayfsPid, SIGTERM);

    return ret;
};

package::Ref fuzzyRef(const JsonSerialize *obj)
{
    if (!obj) {
        return package::Ref("");
    }
    auto id = obj->property("id").toString();
    auto version = obj->property("version").toString();
    auto arch = obj->property("arch").toString();

    package::Ref ref(id);

    if (!version.isEmpty()) {
        ref.version = version;
    }

    if (!arch.isEmpty()) {
        ref.arch = arch;
    } else {
        ref.arch = util::hostArch();
    }

    return ref;
}

linglong::util::Error LinglongBuilder::initRepo()
{
    // if local ostree is not exist, create and init it
    if (!QDir(BuilderConfig::instance()->ostreePath()).exists()) {
        util::ensureDir(BuilderConfig::instance()->ostreePath());

        repo::OSTreeRepo repo(BuilderConfig::instance()->repoPath());

        auto ret = repo.init("bare-user-only");
        if (!ret.success()) {
            return NewError(-1, "init ostree repo failed");
        }

        QString defaultRepoName = "repo";
        QString configUrl = "";

        int statusCode = linglong::util::getLocalConfig("appDbUrl", configUrl);
        if (STATUS_CODE(kSuccess) != statusCode) {
            return NewError() << "call getLocalConfig api failed";
        }

        QString repoUrl = configUrl.endsWith("/") ? configUrl + "repo" : QStringList {configUrl, "repo"}.join("/");

        ret = repo.remoteAdd(defaultRepoName, repoUrl);
        if (!ret.success()) {
            return NewError(-1, "add ostree remote failed");
        }
    }

    return NoError();
}

class BuilderContainer
{
public:
    int startContainer(const Project &project) { return 0; }
};

// FIXME: should merge with runtime
int startContainer(Container *c, Runtime *r)
{
#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

    QList<QList<quint64>> uidMaps = {
        {getuid(), 0, 1},
    };
    for (auto const &uidMap : uidMaps) {
        QPointer<IdMap> idMap(new IdMap(r->linux));
        idMap->hostId = uidMap.value(0);
        idMap->containerId = uidMap.value(1);
        idMap->size = uidMap.value(2);
        r->linux->uidMappings.push_back(idMap);
    }

    QList<QList<quint64>> gidMaps = {
        {getgid(), 0, 1},
    };
    for (auto const &gidMap : gidMaps) {
        QPointer<IdMap> idMap(new IdMap(r->linux));
        idMap->hostId = gidMap.value(0);
        idMap->containerId = gidMap.value(1);
        idMap->size = gidMap.value(2);
        r->linux->gidMappings.push_back(idMap);
    }

    r->root->path = c->workingDirectory + "/root";
    util::ensureDir(r->root->path);

    QProcess process;

    // write pid file
    QFile pidFile(c->workingDirectory + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << r->root->path;
    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(r)).toJson();
    auto data = json.toStdString();

    int sockets[2]; // save file describers of sockets used to communicate with ll-box

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets) != 0) {
        return EXIT_FAILURE;
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    pid_t boxPid = fork();
    if (boxPid < 0) {
        return -1;
    }

    if (0 == boxPid) {
        // child process
        (void)close(sockets[1]);
        auto socket = std::to_string(sockets[0]);
        char const *const args[] = {"/usr/bin/ll-box", socket.c_str(), NULL};
        int ret = execvp(args[0], (char **)args);
        exit(ret);
    } else {
        close(sockets[0]);
        // FIXME: handle error
        (void)write(sockets[1], data.c_str(), data.size());
        (void)write(sockets[1], "\0", 1); // each data write into sockets should ended with '\0'

        c->pid = boxPid;
        waitpid(boxPid, nullptr, 0);
    }
    // return exit status from ll-box
    QString msg;
    const size_t bufSize = 1024;
    char buf[bufSize] = {};
    size_t ret = 0;

    ret = read(sockets[1], buf, bufSize);
    if (ret > 0) {
        msg.append(buf);
    }

    // FIXME: DO NOT name a class as "message" if it not for an common usage.
    QScopedPointer<message> result(util::loadJSONString<message>(msg));

    return result->wstatus;
}

linglong::util::Error LinglongBuilder::create(const QString &projectName)
{
    auto projectPath = QStringList {QDir::currentPath(), projectName}.join("/");
    auto configFilePath = QStringList {projectPath, "linglong.yaml"}.join("/");
    auto templeteFile = QStringList {BuilderConfig::instance()->templatePath(), "template.yaml"}.join("/");

    // TODO: 判断projectName名称合法性
    // 在当前目录创建项目文件夹
    auto ret = QDir().mkpath(projectPath);
    if (!ret) {
        return NewError(-1, "project already exists");
    }

    if (!QFile::copy(templeteFile, configFilePath)) {
        return NewError(-1, "templete file is not found");
    }

    return NoError();
}

linglong::util::Error LinglongBuilder::build()
{
    linglong::util::Error ret(NoError());

    ret = initRepo();
    if (!ret.success()) {
        return NewError(-1, "load local repo failed");
    }

    auto projectConfigPath = QStringList {BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        qCritical() << "ll-builder should running in project root";
        return NewError(-1, "can not found linglong.yaml");
    }

    QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile(projectConfigPath.toStdString())));

    // convert dependencies which with 'source' and 'build' tag to a project type
    // TODO: building dependencies should be concurrency
    for (auto const &depend : project->depends) {
        if (depend->source && depend->build) {
            YAML::Node node;

            node["package"]["kind"] = "lib";
            node["package"]["id"] = depend->id.toStdString();
            node["package"]["version"] = depend->version.toStdString();

            if (project->runtime) {
                node["runtime"]["id"] = project->runtime->id.toStdString();
                node["runtime"]["version"] = project->runtime->version.toStdString();
            }

            if (project->base) {
                node["base"]["id"] = project->base->id.toStdString();
                node["base"]["version"] = project->base->version.toStdString();
            }

            QScopedPointer<Project> subProject(formYaml<Project>(node));

            subProject->variables = depend->variables;
            subProject->source = depend->source;
            subProject->build = depend->build;

            subProject->generateBuildScript();
            subProject->setConfigFilePath(projectConfigPath);

            qInfo() << QString("building target: %1").arg(subProject->package->id);

            ret = buildFlow(subProject.get());
            if (!ret.success()) {
                return ret;
            }

            qInfo() << QString("build %1 success").arg(project->package->id);
        }
    }

    project->generateBuildScript();
    project->setConfigFilePath(projectConfigPath);

    qInfo() << QString("building target: %1").arg(project->package->id);

    ret = buildFlow(project.get());
    if (!ret.success()) {
        return ret;
    }

    qInfo() << QString("build %1 success").arg(project->package->id);
    return NoError();
}

linglong::util::Error LinglongBuilder::buildFlow(Project *project)
{
    linglong::util::Error ret(NoError());

    linglong::builder::BuilderConfig::instance()->setProjectName(project->package->id);

    if (!project->package || project->package->kind.isEmpty()) {
        return NewError(-1, "unknown package kind");
    }

    SourceFetcher sf(project->source, project);
    if (project->source) {
        qInfo() << QString("fetching source code from: %1").arg(project->source->url);
        auto ret = sf.fetch();
        if (!ret.success()) {
            return NewError(-1, "fetch source failed");
        }
    }
    // initialize some directories
    util::removeDir(project->config().cacheRuntimePath({}));
    util::removeDir(project->config().cacheInstallPath(""));
    util::removeDir(project->config().cacheInstallPath("devel-install", ""));
    util::removeDir(project->config().cacheAbsoluteFilePath({"overlayfs"}));
    util::ensureDir(project->config().cacheInstallPath(""));
    util::ensureDir(project->config().cacheInstallPath("devel-install", ""));
    util::ensureDir(
        project->config().cacheAbsoluteFilePath({"overlayfs", "up", project->config().targetInstallPath("")}));

    package::Ref baseRef("");

    QString hostBasePath;
    if (project->base) {
        baseRef = fuzzyRef(project->base);
    }

    if (project->runtime) {
        auto runtimeRef = fuzzyRef(project->runtime);

        BuildDepend runtimeDepend;
        runtimeDepend.id = runtimeRef.appId;
        runtimeDepend.version = runtimeRef.version;
        DependFetcher df(runtimeDepend, project);
        ret = df.fetch("", project->config().cacheRuntimePath(""));
        if (!ret.success()) {
            return NewError(ret, -1, "fetch runtime failed");
        }

        if (!project->base) {
            QFile infoFile(project->config().cacheRuntimePath("info.json"));
            if (!infoFile.open(QIODevice::ReadOnly)) {
                return NewError(infoFile.error(), infoFile.errorString());
            }
            auto info = fromVariant<package::Info>(QJsonDocument::fromJson(infoFile.readAll()).toVariant());

            package::Ref runtimeBaseRef(info->base);

            baseRef.appId = runtimeBaseRef.appId;
            baseRef.version = runtimeBaseRef.version;
        }
    }

    BuildDepend baseDepend;
    baseDepend.id = baseRef.appId;
    baseDepend.version = baseRef.version;
    DependFetcher baseFetcher(baseDepend, project);
    // TODO: the base filesystem hierarchy is just an debian now. we should change it
    hostBasePath = BuilderConfig::instance()->layerPath({baseRef.toLocalRefString(), ""});
    ret = baseFetcher.fetch("", hostBasePath);
    if (!ret.success()) {
        return NewError(ret, -1, "fetch runtime failed");
    }

    // depends fetch
    for (auto const &depend : project->depends) {
        DependFetcher df(*depend, project);
        ret = df.fetch("files", project->config().cacheRuntimePath("files"));
        if (!ret.success()) {
            return NewError(ret, -1, "fetch dependency failed");
        }
    }

    QFile jsonFile(":/config.json");
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    QScopedPointer<Runtime> rt(fromVariant<Runtime>(json.toVariant()));
    auto r = rt.get();

    auto container = new Container(this);
    ret = container->create("");
    if (!ret.success()) {
        return WrapError(ret, "create container failed");
    }

    if (!r->annotations) {
        r->annotations = new Annotations(r);
    }

    if (!r->annotations->overlayfs) {
        r->annotations->overlayfs = new AnnotationsOverlayfsRootfs(r);

        r->annotations->overlayfs->lowerParent = project->config().cacheAbsoluteFilePath({"overlayfs", "lp"});
        r->annotations->overlayfs->workdir = project->config().cacheAbsoluteFilePath({"overlayfs", "wk"});
        r->annotations->overlayfs->upper = project->config().cacheAbsoluteFilePath({"overlayfs", "up"});
    }

    // mount tmpfs to /tmp in container
    QPointer<Mount> m(new Mount(r));
    m->type = "tmpfs";
    m->options = QStringList {"nosuid", "strictatime", "mode=777"};
    m->source = "tmp";
    m->destination = "/tmp";
    r->mounts.push_back(m);

    // get runtime, and parse base from runtime
    for (const auto &rpath : QStringList {"/usr", "/etc", "/var"}) {
        QPointer<Mount> m(new Mount(r));
        m->type = "bind";
        m->options = QStringList {"ro", "rbind"};
        m->source = QStringList {hostBasePath, "files", rpath}.join("/");
        m->destination = rpath;
        r->annotations->overlayfs->mounts.push_back(m);
    }

    // mount project build result path
    // for runtime/lib, use overlayfs
    // for app, mount to opt
    auto hostInstallPath = project->config().cacheInstallPath("files");
    linglong::util::ensureDir(hostInstallPath);
    QList<QPair<QString, QString>> overlaysMountMap = {};

    if (project->runtime || !project->depends.isEmpty()) {
        overlaysMountMap.push_back({project->config().cacheRuntimePath("files"), "/runtime"});
    }

    for (const auto &pair : overlaysMountMap) {
        QPointer<Mount> m(new Mount(r));
        m->type = "bind";
        m->options = QStringList {"ro"};
        m->source = pair.first;
        m->destination = pair.second;
        r->annotations->overlayfs->mounts.push_back(m);
    }
    r->annotations->containerRootPath = container->workingDirectory;

    auto containerSourcePath = "/source";

    QList<QPair<QString, QString>> mountMap = {
        {sf.sourceRoot(), containerSourcePath},
        {project->buildScriptPath(), BuildScriptPath},
    };

    for (const auto &pair : mountMap) {
        QPointer<Mount> m(new Mount(r));
        m->type = "bind";
        m->source = pair.first;
        m->destination = pair.second;
        r->mounts.push_back(m);
    }

    r->process->args = QStringList {"/bin/bash", "-e", BuildScriptPath};
    r->process->cwd = containerSourcePath;
    r->process->env.push_back(
        "PATH=/runtime/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin");
    r->process->env.push_back("PREFIX=" + project->config().targetInstallPath(""));

    if (project->config().targetArch() == "x86_64") {
        r->process->env.push_back("PKG_CONFIG_PATH=/runtime/lib/x86_64-linux-gnu/pkgconfig");
        r->process->env.push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
        r->process->env.push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
    } else if (project->config().targetArch() == "arm64") {
        r->process->env.push_back("PKG_CONFIG_PATH=/runtime/lib/aarch64-linux-gnu/pkgconfig");
        r->process->env.push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
        r->process->env.push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
    }

    if (!r->hooks) {
        r->hooks = new Hooks(r);
        QPointer<Hook> hook(new Hook(r->hooks));
        hook->path = "/usr/sbin/ldconfig";
        r->hooks->prestart.push_back(hook);
    }

    if (startContainer(container, r)) {
        return NewError(-1, "build task failed in container");
    }

    ret = commitBuildOutput(project, r->annotations->overlayfs);
    if (!ret.success()) {
        return NewError(-1, "commitBuildOutput failed");
    }
    return NoError();
}

linglong::util::Error LinglongBuilder::exportBundle(const QString &outputFilePath, bool useLocalDir)
{
    auto exportPath = QStringList {BuilderConfig::instance()->projectRoot(), "export"}.join("/");

    // checkout data from local ostree
    if (!useLocalDir) {
        auto projectConfigPath = QStringList {BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");
        if (!QFileInfo::exists(projectConfigPath)) {
            qCritical() << "ll-builder should running in project root";
            return NewError(-1, "can not found linglong.yaml");
        }

        QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile(projectConfigPath.toStdString())));
        project->setConfigFilePath(projectConfigPath);

        exportPath = QStringList {BuilderConfig::instance()->projectRoot(), project->package->id}.join("/");

        util::ensureDir(exportPath);

        repo::OSTreeRepo repo(BuilderConfig::instance()->repoPath());
        auto ret = repo.checkout(project->refWithModule("runtime"), "", QStringList {exportPath, "runtime"}.join("/"));
        ret = repo.checkout(project->refWithModule("devel"), "", QStringList {exportPath, "devel"}.join("/"));
        if (!ret.success()) {
            return NewError(-1, "checkout files failed, you need build first");
        }
    }
    // TODO: for bundle. remove later
    QFile::copy(QStringList {exportPath, "runtime", "info.json"}.join("/"),
                QStringList {exportPath, "info.json"}.join("/"));
    // TODO: if the kind is not app, don't make bundle
    // make bundle package
    linglong::package::Bundle uabBundle;

    auto makeBundleResult = uabBundle.make(exportPath, outputFilePath);
    if (!makeBundleResult.success()) {
        return NewError(-1, "make bundle failed");
    }

    return NoError();
}

linglong::util::Error LinglongBuilder::push(const QString &ref)
{
    repo::OSTreeRepo repo(BuilderConfig::instance()->repoPath(), BuilderConfig::instance()->remoteRepoEndpoint,
                          BuilderConfig::instance()->remoteRepoName);
    repo.push(package::Ref(ref), false);

    return linglong::util::Error(NoError());
}

linglong::util::Error LinglongBuilder::push(const QString &bundleFilePath, const QString &repoUrl,
                                            const QString &repoChannel, bool force)
{
    // TODO: if the kind is not app, don't push bundle
    linglong::package::Bundle uabBundle;

    auto pushBundleResult = uabBundle.push(bundleFilePath, repoUrl, repoChannel, force);
    if (!pushBundleResult.success()) {
        return NewError(-1, "push bundle failed");
    }

    return NoError();
}

linglong::util::Error LinglongBuilder::run()
{
    repo::OSTreeRepo repo(BuilderConfig::instance()->repoPath());

    linglong::util::Error ret(NoError());

    auto projectConfigPath = QStringList {BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        qCritical() << "ll-builder should running in project root";
        return NewError(-1, "can not found linglong.yaml");
    }

    QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile(projectConfigPath.toStdString())));
    project->setConfigFilePath(projectConfigPath);

    // checkout app
    auto targetPath = BuilderConfig::instance()->layerPath({project->ref().toLocalRefString()});
    linglong::util::ensureDir(targetPath);
    ret = repo.checkoutAll(project->ref(), "", targetPath);
    if (!ret.success()) {
        return NewError(-1, "checkout app files failed");
    }

    // checkout runtime
    targetPath = BuilderConfig::instance()->layerPath({project->runtimeRef().toLocalRefString()});
    linglong::util::ensureDir(targetPath);

    auto remoteRuntimeRef = package::Ref("", "linglong", project->runtimeRef().appId, project->runtimeRef().version,
                                         project->runtimeRef().arch);

    ret = repo.checkoutAll(remoteRuntimeRef, "", targetPath);
    if (!ret.success()) {
        ret = repo.checkoutAll(project->runtimeRef(), "", targetPath);
    }

    if (!ret.success()) {
        return NewError(-1, "checkout runtime files failed");
    }

    // 获取环境变量
    QStringList userEnvList = COMMAND_HELPER->getUserEnv(linglong::util::envList);

    auto app = runtime::App::load(&repo, project->ref(), BuilderConfig::instance()->exec(), false);
    if (nullptr == app) {
        return NewError(-1, "load App::load failed");
    }
    app->saveUserEnvList(userEnvList);
    app->start();
    return ret;
}

} // namespace builder
} // namespace linglong
