/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "builder_config.h"
#include "depend_fetcher.h"
#include "linglong/package/bundle.h"
#include "linglong/package/info.h"
#include "linglong/package/layer_packager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/app.h"
#include "linglong/runtime/container.h"
#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/runner.h"
#include "linglong/util/sysinfo.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/std_helper/qdebug_helper.h"
#include "linglong/utils/xdg/desktop_entry.h"
#include "ocppi/runtime/config/ConfigLoader.hpp"
#include "ocppi/runtime/config/types/Hook.hpp"
#include "ocppi/runtime/config/types/Hooks.hpp"
#include "project.h"
#include "source_fetcher.h"

#include <linux/prctl.h>
#include <ocppi/runtime/config/types/IdMapping.hpp>
#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QDir>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>

#include <csignal>
#include <fstream>
#include <optional>

#include <sys/socket.h>
#include <sys/wait.h>

namespace linglong::builder {

QSERIALIZER_IMPL(message)

LinglongBuilder::LinglongBuilder(repo::OSTreeRepo &ostree, cli::Printer &p)
    : repo(ostree)
    , printer(p)
{
}

linglong::utils::error::Result<void>
LinglongBuilder::commitBuildOutput(Project *project, const nlohmann::json &overlayfs)
{
    auto output = project->config().cacheInstallPath("files");
    linglong::util::ensureDir(output);

    auto upperDir = QStringList{ QString::fromStdString(overlayfs["upper"]),
                                 project->config().targetInstallPath("") }
                      .join("/");
    linglong::util::ensureDir(upperDir);

    auto lowerDir = project->config().cacheAbsoluteFilePath({ "overlayfs", "lower" });
    // if combine runtime, lower is ${PROJECT_CACHE}/runtime/files
    if (PackageKindRuntime == project->package->kind) {
        lowerDir = project->config().cacheAbsoluteFilePath({ "runtime", "files" });
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
      QString::fromStdString("workdir=" + std::string(overlayfs["workdir"])),
      "-o",
      "lowerdir=" + lowerDir,
      output,
    });

    qDebug() << "run" << fuseOverlayfs.program() << fuseOverlayfs.arguments().join(" ");

    qint64 fuseOverlayfsPid = -1;
    fuseOverlayfs.startDetached(&fuseOverlayfsPid);
    fuseOverlayfs.waitForStarted(-1);

    auto _ = qScopeGuard([=] {
        kill(static_cast<pid_t>(fuseOverlayfsPid), SIGTERM);
    });

    // FIXME: must wait fuse mount filesystem
    QThread::sleep(1);

    auto entriesPath = project->config().cacheInstallPath("entries");

    auto modifyConfigFile = [](const QString &srcPath,
                               const QString &targetPath,
                               const QString &fileType,
                               const QString &appId) -> util::Error {
        QDir configDir(srcPath);
        auto configFileInfoList = configDir.entryInfoList({ fileType }, QDir::Files);

        linglong::util::ensureDir(targetPath);

        for (auto const &fileInfo : configFileInfoList) {
            auto desktopEntry = utils::xdg::DesktopEntry::New(fileInfo.filePath());
            if (!desktopEntry.has_value()) {
                return NewError(desktopEntry.error().code(), "file to config not exists");
            }

            // set all section
            auto configSections = desktopEntry->groups();
            for (auto section : configSections) {
                auto exec = desktopEntry->getValue<QString>("Exec", section);
                if (exec.has_value()) {
                    // TODO(black_desk): update to new CLI
                    exec = QString("ll-cli run %1 --exec %2").arg(appId, *exec);
                    desktopEntry->setValue("Exec", *exec, section);
                }

                // The section TryExec affects starting from the launcher, set it to null.
                auto tryExec = desktopEntry->getValue<QString>("TryExec", section);

                if (tryExec.has_value()) {
                    desktopEntry->setValue<QString>("TryExec", BINDIR "/ll-cli");
                }

                desktopEntry->setValue<QString>("X-linglong", appId);

                auto result = desktopEntry->saveToFile(
                  QStringList{ targetPath, fileInfo.fileName() }.join(QDir::separator()));
                if (!result.has_value()) {
                    return NewError(result.error().code(), result.error().message());
                }
            }
        }
        return Success();
    };

    auto appId = project->package->id;
    // modify desktop file
    auto desktopFilePath = QStringList{ output, "share/applications" }.join(QDir::separator());
    auto desktopFileSavePath = QStringList{ entriesPath, "applications" }.join(QDir::separator());
    auto err = modifyConfigFile(desktopFilePath, desktopFileSavePath, "*.desktop", appId);
    if (err) {
        return LINGLONG_ERR(err.code(), err.message());
    }

    // modify context-menus file
    auto contextFilePath =
      QStringList{ output, "share/applications/context-menus" }.join(QDir::separator());
    auto contextFileSavePath = contextFilePath;
    err = modifyConfigFile(contextFilePath, contextFileSavePath, "*.conf", appId);
    if (err) {
        return LINGLONG_ERR(err.code(), err.message());
    }

    auto moveDir = [](const QStringList targetList,
                      const QString &srcPath,
                      const QString &destPath) -> linglong::util::Error {
        for (auto target : targetList) {
            auto srcDir = QStringList{ srcPath, target }.join(QDir::separator());
            auto destDir = QStringList{ destPath, target }.join(QDir::separator());

            if (util::dirExists(srcDir)) {
                util::copyDir(srcDir, destDir);
                util::removeDir(srcDir);
            }
        }

        return Success();
    };

    // link files/share to entries/share/
    linglong::util::linkDirFiles(project->config().cacheInstallPath("files/share"), entriesPath);
    // link files/lib/systemd to entries/systemd
    linglong::util::linkDirFiles(
      project->config().cacheInstallPath("files/lib/systemd/user"),
      QStringList{ entriesPath, "systemd/user" }.join(QDir::separator()));

    // Move runtime-install/files/debug to devel-install/files/debug
    linglong::util::ensureDir(project->config().cacheInstallPath("devel-install", "files"));
    err = moveDir({ "debug", "include", "mkspec ", "cmake", "pkgconfig" },
                  project->config().cacheInstallPath("files"),
                  project->config().cacheInstallPath("devel-install", "files"));
    if (err) {
        return LINGLONG_ERR(err.code(), err.message());
    }

    auto createInfo = [](Project *project) -> linglong::util::Error {
        QSharedPointer<package::Info> info(new package::Info);

        info->kind = project->package->kind;
        info->version = project->package->version;

        info->base = project->baseRef().toLocalRefString();

        info->runtime = project->runtimeRef().toLocalRefString();

        info->appid = project->package->id;
        info->name = project->package->name;
        info->description = project->package->description;
        info->arch = QStringList{ project->config().targetArch() };

        info->module = "runtime";
        info->size = linglong::util::sizeOfDir(project->config().cacheInstallPath(""));
        QFile infoFile(project->config().cacheInstallPath("info.json"));
        if (!infoFile.open(QIODevice::WriteOnly)) {
            return NewError(infoFile.error(), infoFile.errorString() + " " + infoFile.fileName());
        }

        // FIXME(black_desk): handle error
        if (infoFile.write(std::get<0>(util::toJSON(info))) < 0) {
            return NewError(infoFile.error(), infoFile.errorString());
        }
        infoFile.close();

        info->module = "devel";
        info->size = linglong::util::sizeOfDir(project->config().cacheInstallPath("devel-install"));
        QFile develInfoFile(project->config().cacheInstallPath("devel-install", "info.json"));
        if (!develInfoFile.open(QIODevice::WriteOnly)) {
            return NewError(develInfoFile.error(),
                            develInfoFile.errorString() + " " + develInfoFile.fileName());
        }
        if (develInfoFile.write(std::get<0>(util::toJSON(info))) < 0) {
            return NewError(develInfoFile.error(), develInfoFile.errorString());
        }
        develInfoFile.close();
        QFile sourceConfigFile(project->configFilePath);
        if (!sourceConfigFile.copy(project->config().cacheInstallPath("linglong.yaml"))) {
            return NewError(sourceConfigFile.error(), sourceConfigFile.errorString());
        }

        return Success();
    };

    err = createInfo(project);
    if (err) {
        return LINGLONG_ERR(err.code(), err.message());
    }
    auto refRuntime = project->refWithModule("runtime");
    refRuntime.channel = "main";
    auto ret = repo.importDirectory(refRuntime, project->config().cacheInstallPath(""));

    if (!ret.has_value()) {
        printer.printMessage(
          QString("commit %1 filed").arg(project->refWithModule("runtime").toString()));
        return LINGLONG_EWRAP("commit runtime filed", ret.error());
    }

    auto refDevel = project->refWithModule("devel");
    refDevel.channel = "main";
    ret = repo.importDirectory(refDevel, project->config().cacheInstallPath("devel-install", ""));
    if (!ret.has_value())
        return LINGLONG_EWRAP("commit devel filed", ret.error());
    return LINGLONG_OK;
};

package::Ref fuzzyRef(QSharedPointer<const JsonSerialize> obj)
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

linglong::util::Error LinglongBuilder::config(const QString &userName, const QString &password)
{
    auto userJsonData =
      QString("{\"username\": \"%1\",\n \"password\": \"%2\"}").arg(userName).arg(password);

    QFile infoFile(util::getUserFile(".linglong/.user.json"));
    if (!infoFile.open(QIODevice::WriteOnly)) {
        return NewError(infoFile.error(), infoFile.errorString() + " " + infoFile.fileName());
    }
    if (infoFile.write(userJsonData.toLocal8Bit()) < 0) {
        return NewError(infoFile.error(), infoFile.errorString());
    }
    infoFile.close();

    return Success();
}

// FIXME: should merge with runtime
int LinglongBuilder::startContainer(QSharedPointer<Container> c,
                                    ocppi::runtime::config::types::Config &r)
{

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

    QList<QList<int64_t>> uidMaps = {
        { getuid(), 0, 1 },
    };
    for (auto const &uidMap : uidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = uidMap.value(0);
        idMap.containerID = uidMap.value(1);
        idMap.size = uidMap.value(2);
        r.linux_->uidMappings =
          r.linux_->uidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>());
        r.linux_->uidMappings->push_back(idMap);
    }

    QList<QList<quint64>> gidMaps = {
        { getgid(), 0, 1 },
    };
    for (auto const &gidMap : gidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = gidMap.value(0);
        idMap.containerID = gidMap.value(1);
        idMap.size = gidMap.value(2);
        r.linux_->gidMappings =
          r.linux_->gidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>());
        r.linux_->gidMappings->push_back(idMap);
    }

    r.root->path = c->workingDirectory.toStdString() + "/root";
    util::ensureDir(r.root->path.c_str());
    // write pid file
    QFile pidFile(c->workingDirectory + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << r.root->path;

    auto runtimeConfigJSON = toJSON(r);

    auto data = runtimeConfigJSON.dump();

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
        char const *const args[] = { "ll-box", socket.c_str(), NULL };
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
    auto result = util::loadJsonString<message>(msg);

    return result->wstatus;
}

linglong::util::Error LinglongBuilder::generate(const QString &projectName)
{
    auto projectPath = QStringList{ QDir::currentPath(), projectName }.join(QDir::separator());
    auto configFilePath = QStringList{ projectPath, "linglong.yaml" }.join(QDir::separator());
    auto templeteFilePath =
      QStringList{ BuilderConfig::instance()->templatePath(), "appimage.yaml" }.join(
        QDir::separator());

    auto ret = QDir().mkpath(projectPath);
    if (!ret) {
        return NewError(-1, "project already exists");
    }
    if (QFileInfo::exists(templeteFilePath)) {
        QFile::copy(templeteFilePath, configFilePath);
    } else {
        QFile::copy(":/appimage.yaml", configFilePath);
    }

    return Success();
}

linglong::util::Error LinglongBuilder::create(const QString &projectName)
{
    auto projectPath = QStringList{ QDir::currentPath(), projectName }.join(QDir::separator());
    auto configFilePath = QStringList{ projectPath, "linglong.yaml" }.join(QDir::separator());
    auto templeteFilePath =
      QStringList{ BuilderConfig::instance()->templatePath(), "example.yaml" }.join(
        QDir::separator());

    auto ret = QDir().mkpath(projectPath);
    if (!ret) {
        return NewError(-1, "project already exists");
    }

    if (QFileInfo::exists(templeteFilePath)) {
        QFile::copy(templeteFilePath, configFilePath);
    } else {
        QFile::copy(":/example.yaml", configFilePath);
    }

    return Success();
}

linglong::util::Error LinglongBuilder::track()
{
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join(
        QDir::separator());

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }

    auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
    if (err) {
        return WrapError(err, "cannot load project yaml");
    }

    if ("git" == project->source->kind) {
        auto gitOutput = QSharedPointer<QByteArray>::create();
        QString latestCommit;
        // default git ref is HEAD
        auto gitRef = project->source->version.isEmpty() ? "HEAD" : project->source->version;
        auto err = util::Exec("git", { "ls-remote", project->source->url, gitRef }, -1, gitOutput);
        if (!err) {
            latestCommit = QString::fromLocal8Bit(*gitOutput).trimmed().split("\t").first();
            qDebug() << latestCommit;

            if (project->source->commit == latestCommit) {
                printer.printMessage("current commit is the latest, nothing to update");
            } else {
                std::ofstream fout(projectConfigPath.toStdString());
                auto result = util::toYAML(project);
                fout << std::get<0>(result).toStdString();
                printer.printMessage("update commit to:" + latestCommit);
            }
        }
    }

    return Success();
}

linglong::util::Error LinglongBuilder::build()
{
    linglong::util::Error err;
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }
    auto [project, ret] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
    if (ret) {
        return WrapError(ret, "cannot load project yaml");
    }

    project->generateBuildScript();
    project->configFilePath = projectConfigPath;
    linglong::builder::BuilderConfig::instance()->setProjectName(project->package->id);

    printer.printMessage("[Build Target]");
    printer.printMessage(project->package->id, 2);
    printer.printMessage("[Project Info]");
    printer.printMessage(QString("Packge Name: %1").arg(project->package->name), 2);
    printer.printMessage(QString("Version: %1").arg(project->package->version), 2);
    printer.printMessage(QString("Packge Type: %1").arg(project->package->kind), 2);
    printer.printMessage(QString("Build Arch: %1").arg(project->config().targetArch()), 2);

    printer.printMessage("[Current Repo]");
    printer.printMessage(QString("Name: %1").arg(BuilderConfig::instance()->remoteRepoName), 2);
    printer.printMessage(QString("Url: %1").arg(BuilderConfig::instance()->remoteRepoEndpoint), 2);

    if (!project->package || project->package->kind.isEmpty()) {
        return NewError(-1, "unknown package kind");
    }

    SourceFetcher sf(project->source, printer, project.get());
    if (project->source) {
        printer.printMessage("[Processing Source]");
        auto err = sf.fetch();
        if (err) {
            return NewError(-1, "fetch source failed");
        }
    }
    // initialize some directories
    util::removeDir(project->config().cacheRuntimePath({}));
    util::removeDir(project->config().cacheInstallPath(""));
    util::removeDir(project->config().cacheInstallPath("devel-install", ""));
    util::removeDir(project->config().cacheAbsoluteFilePath({ "overlayfs" }));
    util::ensureDir(project->config().cacheInstallPath(""));
    util::ensureDir(project->config().cacheInstallPath("devel-install", ""));
    util::ensureDir(project->config().cacheAbsoluteFilePath(
      { "overlayfs", "up", project->config().targetInstallPath("") }));

    printer.printMessage("[Processing Dependency]");
    printer.printMessage(
      QString("%1%2%3%4").arg("Package", -20).arg("Version", -15).arg("Module", -15).arg("Status"),
      2);
    package::Ref baseRef("");

    QString hostBasePath;
    if (project->base) {
        baseRef = fuzzyRef(project->base);
    }

    if (project->runtime) {
        auto runtimeRef = fuzzyRef(project->runtime);

        QSharedPointer<BuildDepend> runtimeDepend(new BuildDepend);
        runtimeDepend->id = runtimeRef.appId;
        runtimeDepend->version = runtimeRef.version;
        DependFetcher df(runtimeDepend, repo, printer, project.get());
        err = df.fetch("", project->config().cacheRuntimePath(""));
        if (err) {
            return WrapError(err, "fetch runtime failed");
        }

        if (!project->base) {
            QFile infoFile(project->config().cacheRuntimePath("info.json"));
            if (!infoFile.open(QIODevice::ReadOnly)) {
                return NewError(infoFile.error(), infoFile.errorString());
            }
            // FIXME(black_desk): handle error
            auto info =
              std::get<0>(util::fromJSON<QSharedPointer<package::Info>>(infoFile.readAll()));

            package::Ref runtimeBaseRef(info->base);

            baseRef.appId = runtimeBaseRef.appId;
            baseRef.version = runtimeBaseRef.version;
        }
    }

    QSharedPointer<BuildDepend> baseDepend(new BuildDepend);
    baseDepend->id = baseRef.appId;
    baseDepend->version = baseRef.version;
    DependFetcher baseFetcher(baseDepend, repo, printer, project.get());
    // TODO: the base filesystem hierarchy is just an debian now. we should change it
    hostBasePath = BuilderConfig::instance()->layerPath({ baseRef.toLocalRefString(), "" });
    err = baseFetcher.fetch("", hostBasePath);
    if (err) {
        return WrapError(err, "fetch base failed");
    }

    // depends fetch
    for (auto const &depend : project->depends) {
        DependFetcher df(depend, repo, printer, project.get());
        err = df.fetch("files", project->config().cacheRuntimePath("files"));
        if (err) {
            return WrapError(err, "fetch dependency failed");
        }
    }

    // FIXME(black_desk): these code should not be written in here. We should
    // use code in runtime/app.cpp
    //

    QFile builtinOCIConfigTemplateJSONFile(":/config.json");
    if (!builtinOCIConfigTemplateJSONFile.open(QIODevice::ReadOnly)) {
        // NOTE(black_desk): Go check qrc if this occurs.
        qFatal("builtin OCI configuration template file missing.");
    }

    auto bytes = builtinOCIConfigTemplateJSONFile.readAll();
    std::stringstream tmpStream;
    tmpStream << bytes.toStdString();

    ocppi::runtime::config::ConfigLoader loader;
    auto r = loader.load(tmpStream);
    if (!r.has_value()) {
        printer.printMessage("builtin OCI configuration template is invalid.");
        try {
            std::rethrow_exception(r.error());
        } catch (const std::exception &e) {
            qFatal("%s", e.what());
        } catch (...) {
            qFatal("Unknown error");
        }
    }

    auto container = QSharedPointer<Container>(new Container);
    err = container->create("");
    if (err) {
        return WrapError(err, "create container failed");
    }

    r->annotations = r->annotations.value_or(std::map<std::string, nlohmann::json>{});
    auto &anno = *r->annotations;

    anno["overlayfs"]["lowerParent"] =
      project->config().cacheAbsoluteFilePath({ "overlayfs", "lp" }).toStdString();

    anno["overlayfs"]["upper"] =
      project->config().cacheAbsoluteFilePath({ "overlayfs", "up" }).toStdString();

    anno["overlayfs"]["workdir"] =
      project->config().cacheAbsoluteFilePath({ "overlayfs", "wk" }).toStdString();

    // mount tmpfs to /tmp in container
    ocppi::runtime::config::types::Mount m;
    m.type = "tmpfs";
    m.options = { "nosuid", "strictatime", "mode=777" };
    m.source = "tmp";
    m.destination = "/tmp";
    r->mounts->push_back(m);

    auto &mounts = anno["overlayfs"]["mounts"] = nlohmann::json::array_t{};

    // get runtime, and parse base from runtime
    for (const auto &rpath : QStringList{ "/usr", "/etc", "/var" }) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "ro", "rbind" };
        m.source = QStringList{ hostBasePath, "files", rpath }.join("/").toStdString();
        m.destination = rpath.toStdString();
        mounts.push_back(toJSON(m));
    }

    // mount project build result path
    // for runtime/lib, use overlayfs
    // for app, mount to opt
    auto hostInstallPath = project->config().cacheInstallPath("files");
    linglong::util::ensureDir(hostInstallPath);
    QList<QPair<QString, QString>> overlaysMountMap = {};

    if (project->runtime || !project->depends.isEmpty()) {
        overlaysMountMap.push_back({ project->config().cacheRuntimePath("files"), "/runtime" });
    }

    for (const auto &pair : overlaysMountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.options = { "ro" };
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        mounts.push_back(toJSON(m));
    }

    anno["containerRootPath"] = container->workingDirectory.toStdString();

    auto containerSourcePath = "/source";

    QList<QPair<QString, QString>> mountMap = {
        { sf.sourceRoot(), containerSourcePath },
        { project->buildScriptPath(), BuildScriptPath },
    };

    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        r->mounts->push_back(m);
    }

    r->process->args = { "/bin/bash", "-e", BuildScriptPath };
    r->process->cwd = containerSourcePath;
    r->process->env->push_back("PATH=/runtime/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/"
                               "usr/games:/sbin:/usr/sbin");
    r->process->env->push_back(("PREFIX=" + project->config().targetInstallPath("")).toStdString());

    if (project->config().targetArch() == "x86_64") {
        r->process->env->push_back("PKG_CONFIG_PATH=/runtime/lib/x86_64-linux-gnu/pkgconfig");
        r->process->env->push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
        r->process->env->push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
    } else if (project->config().targetArch() == "arm64") {
        r->process->env->push_back("PKG_CONFIG_PATH=/runtime/lib/aarch64-linux-gnu/pkgconfig");
        r->process->env->push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
        r->process->env->push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
    }

    r->hooks = r->hooks.value_or(ocppi::runtime::config::types::Hooks{});
    r->hooks->prestart = { { std::nullopt, std::nullopt, "/usr/sbin/ldconfig", std::nullopt } };

    printer.printMessage("[Start Build]");
    if (startContainer(container, *r)) {
        return NewError(-1, "build task failed in container");
    }
    printer.printMessage("[Commit Content]");
    auto result = commitBuildOutput(project.get(), anno["overlayfs"]);
    if (!result.has_value()) {
        return NewError(result.error().code(), result.error().message());
    }

    printer.printMessage(QString("Build %1 success").arg(project->package->id));
    return Success();
}

linglong::util::Error LinglongBuilder::exportLayer(const QString &destination)
{
    QDir destDir(destination);

    if (!destDir.exists()) {
        return NewError(-1, QString("the directory '%1' is not exist").arg(destDir.dirName()));
    }

    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");
    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }

    auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
    if (err) {
        return WrapError(err, "cannot load project yaml");
    }
    project->configFilePath = projectConfigPath;

    const auto runtimePath = QStringList{ destination, ".linglong-runtime" }.join("/");
    const auto develPath = QStringList{ destination, ".linglong-devel" }.join("/");

    util::ensureDir(runtimePath);
    util::ensureDir(develPath);

    auto runtimeRef = project->refWithModule("runtime");
    auto develRef = project->refWithModule("devel");
    auto ret = repo.checkout(runtimeRef, "", runtimePath);
    if (!ret.has_value()) {
        qDebug() << ret.error().code() << ret.error().message();
        return WrapError(NewError(ret.error().code(), ret.error().message()),
                         "checkout files with runtime failed, you need build first");
    }
    ret = repo.checkout(develRef, "", develPath);
    if (!ret.has_value()) {
        return WrapError(NewError(ret.error().code(), ret.error().message()),
                         "checkout files with devel failed, you need build first");
    }
    package::LayerDir runtimeDir(runtimePath);
    package::LayerDir develDir(develPath);

    // appid_version_arch_module.layer
    const auto runtimeLayerPath = QString("%1/%2_%3_%4_%5.layer")
                                    .arg(destDir.absolutePath())
                                    .arg(runtimeRef.appId)
                                    .arg(runtimeRef.version)
                                    .arg(runtimeRef.arch)
                                    .arg(runtimeRef.module);

    const auto develLayerPath = QString("%1/%2_%3_%4_%5.layer")
                                  .arg(destDir.absolutePath())
                                  .arg(develRef.appId)
                                  .arg(develRef.version)
                                  .arg(develRef.arch)
                                  .arg(develRef.module);
    package::LayerPackager pkg;
    auto runtimeLayer = pkg.pack(runtimeDir, runtimeLayerPath);
    if (!runtimeLayer.has_value()) {
        return WrapError(NewError(runtimeLayer.error().code(), runtimeLayer.error().message()),
                         "pack runtime layer failed");
    }
    auto develLayer = pkg.pack(develDir, develLayerPath);
    if (!develLayer.has_value()) {
        return WrapError(NewError(develLayer.error().code(), develLayer.error().message()),
                         "pack devel layer failed");
    }

    return Success();
}

linglong::util::Error LinglongBuilder::extractLayer(const QString &layerPath,
                                                    const QString &destination)
{
    qDebug() << layerPath << destination;
    const auto layerFile = package::LayerFile::openLayer(layerPath);
    if (!layerFile.has_value()) {
        return WrapError(NewError(layerFile.error().code(), layerFile.error().message()),
                         "failed to open layer file");
    }

    util::ensureDir(destination);

    package::LayerPackager pkg;
    const auto layerDir = pkg.unpack(*(*layerFile), destination);
    if (!layerDir.has_value()) {
        return WrapError(NewError(layerDir.error().code(), layerDir.error().message()),
                         "failed to unpack layer file");
    }

    (*layerDir)->setCleanStatus(false);
    return Success();
}

linglong::util::Error LinglongBuilder::exportBundle(const QString &outputFilePath,
                                                    bool /*useLocalDir*/)
{
    // checkout data from local ostree
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");
    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }
    auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
    if (err) {
        return WrapError(err, "cannot load project yaml");
    }
    project->configFilePath = projectConfigPath;

    const auto exportPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), project->package->id }.join("/");

    util::ensureDir(exportPath);

    auto ret = repo.checkout(project->refWithModule("runtime"), "", exportPath);
    if (!ret.has_value()) {
        return WrapError(NewError(ret.error().code(), ret.error().message()),
                         "checkout files with runtime failed, you need build first");
    }
    ret = repo.checkout(project->refWithModule("devel"),
                        "",
                        QStringList{ exportPath, "devel" }.join("/"));
    if (!ret.has_value()) {
        return WrapError(NewError(ret.error().code(), ret.error().message()),
                         "checkout files with devel failed, you need build first");
    }

    QFile::copy("/usr/libexec/ll-box-static", QStringList{ exportPath, "ll-box" }.join("/"));

    auto loaderFilePath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "loader" }.join("/");
    if (util::fileExists(loaderFilePath)) {
        QFile::copy(loaderFilePath, QStringList{ exportPath, "loader" }.join("/"));
    } else {
        printer.printMessage("ll-builder need loader to build a runnable uab");
    }

    package::Ref baseRef("");

    if (!project->base) {
        auto info = std::get<0>(util::fromJSON<QSharedPointer<package::Info>>(
          project->config().cacheRuntimePath("info.json")));

        package::Ref runtimeBaseRef(info->base);

        baseRef.appId = runtimeBaseRef.appId;
        baseRef.version = runtimeBaseRef.version;
    }

    const auto extraFileListPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "extra_files.txt" }.join("/");
    if (util::fileExists(extraFileListPath)) {
        auto copyExtraFile = [baseRef, exportPath, &project](const QString &path) -> util::Error {
            QString containerFilePath;
            QString filePath;
            QString exportFilePath;

            if (path.startsWith("/runtime")) {
                filePath = project->config().cacheRuntimePath(
                  QStringList{ "files", path.right(path.length() - sizeof("/runtime")) }.join("/"));

                exportFilePath = QStringList{ exportPath, "lib", path }.join("/");
            } else {
                filePath = BuilderConfig::instance()->layerPath(
                  { baseRef.toLocalRefString(), "files", path });
                exportFilePath = QStringList{ exportPath, "lib", path }.join("/");
            }

            if (!util::fileExists(filePath) && !util::dirExists(filePath)) {
                return NewError(-1, QString("file %1 not exist").arg(filePath));
            }

            // ensure parent path
            util::ensureDir(exportFilePath);
            util::removeDir(exportFilePath);

            QProcess p;
            p.setProgram("cp");
            p.setArguments({ "-Pr", filePath, exportFilePath }); // use cp -Pr to keep symlink
            if (!p.startDetached()) {
                return NewError(-1, "failed to start cp");
            }
            p.waitForStarted(1000);
            p.waitForFinished(1000 * 60 * 60); // one hour
            if (p.exitCode() != 0) {
                return NewError(
                  -1,
                  QString("failed to copy %1 to  %2 ").arg(filePath).arg(exportFilePath));
            }
            return Success();
        };

        QFile extraFileList(extraFileListPath);
        if (!extraFileList.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return NewError(-1, "cannot open extra_files.txt");
        }
        QTextStream textStream(&extraFileList);
        while (true) {
            QString line = textStream.readLine();
            if (line.isNull()) {
                break;

            } else {
                auto err = copyExtraFile(line);
                if (err) {
                    return err;
                }
            }
        }

        if (!extraFileList.copy(QStringList{ exportPath, "lib", "extra_files.txt" }.join("/"))) {
            return NewError(-1, "cannot copy extra_files.txt");
        }
    }

    // TODO: if the kind is not app, don't make bundle
    // make bundle package
    linglong::package::Bundle uabBundle;

    err = uabBundle.make(exportPath, outputFilePath);
    if (err) {
        return WrapError(err, "make bundle failed");
    }

    return Success();
}

util::Error LinglongBuilder::push(const QString &repoUrl,
                                  const QString &repoName,
                                  const QString &channel,
                                  bool pushWithDevel)
{
    auto ret = Success();
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }

    try {
        auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
        if (err) {
            return WrapError(err, "cannot load project yaml");
        }
        // set remote repo url
        auto remoteRepoEndpoint =
          repoUrl.isEmpty() ? BuilderConfig::instance()->remoteRepoEndpoint : repoUrl;
        auto remoteRepoName =
          repoName.isEmpty() ? BuilderConfig::instance()->remoteRepoName : repoName;

        // Fixme: should be buildArch.
        auto refWithRuntime = package::Ref("",
                                           channel,
                                           project->package->id,
                                           project->package->version,
                                           util::hostArch(),
                                           "runtime");
        auto refWithDevel = package::Ref("",
                                         channel,
                                         project->package->id,
                                         project->package->version,
                                         util::hostArch(),
                                         "devel");

        // push ostree data by ref
        // ret = repo.push(refWithRuntime, false);
        auto ret = repo.push(refWithRuntime);

        if (!ret.has_value()) {
            printer.printMessage(QString("push %1 failed").arg(project->package->id));
            return NewError(ret.error().code(), ret.error().message());
        } else {
            printer.printMessage(QString("push %1 success").arg(project->package->id));
        }

        if (pushWithDevel) {
            auto ret = repo.push(package::Ref(refWithDevel));

            if (!ret.has_value()) {
                printer.printMessage(QString("push %1 failed").arg(project->package->id));
            } else {
                printer.printMessage(QString("push %1 success").arg(project->package->id));
            }
        }
    } catch (std::exception &e) {
        return NewError(-1, "failed to parse linglong.yaml. " + QString(e.what()));
    }

    return ret;
}

util::Error LinglongBuilder::importLayer(const QString &path)
{
    const auto layerFile = package::LayerFile::openLayer(path);
    if (!layerFile.has_value()) {
        return WrapError(NewError(layerFile.error().code(), layerFile.error().message()),
                         "failed to open layer file");
    }

    const auto workDir = QStringList{ BuilderConfig::instance()->repoPath(),
                                      QUuid::createUuid().toString(QUuid::Id128) }
                           .join(QDir::separator());
    util::ensureDir(workDir);

    package::LayerPackager pkg;
    const auto layerDir = pkg.unpack(*(*layerFile), workDir);
    if (!layerDir.has_value()) {
        return WrapError(NewError(layerDir.error().code(), layerDir.error().message()),
                         "failed to unpack layer file");
    }

    const auto pkgInfo = *((*layerDir)->info());
    const auto ref = package::Ref("",
                                  "main",
                                  pkgInfo->appid,
                                  pkgInfo->version,
                                  pkgInfo->arch.first(),
                                  pkgInfo->module);
    auto ret = repo.importDirectory(ref, (*layerDir)->absolutePath());
    if (!ret.has_value()) {
        return WrapError(NewError(ret.error().code(), ret.error().message()),
                         "failed to unpack layer file");
    }

    return Success();
}

util::Error LinglongBuilder::import()
{
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }

    try {
        auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
        if (err) {
            return WrapError(err, "cannot load project yaml");
        }

        auto refWithRuntime = package::Ref("",
                                           project->package->id,
                                           project->package->version,
                                           util::hostArch(),
                                           "runtime");

        auto ret =
          repo.importDirectory(refWithRuntime, BuilderConfig::instance()->getProjectRoot());

        if (!ret.has_value()) {
            return NewError(-1, "import package failed");
        }

        printer.printMessage(
          QString("import %1 success").arg(refWithRuntime.toOSTreeRefLocalString()));
    } catch (std::exception &e) {
        return NewError(-1, "failed to parse linglong.yaml. " + QString(e.what()));
    }

    return Success();
}

linglong::util::Error LinglongBuilder::run()
{
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return NewError(-1, "linglong.yaml not found");
    }

    try {
        auto [project, err] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
        if (err) {
            return WrapError(err, "cannot load project yaml");
        }
        project->configFilePath = projectConfigPath;

        // checkout app
        auto targetPath =
          BuilderConfig::instance()->layerPath({ project->ref().toLocalRefString() });
        linglong::util::ensureDir(targetPath);
        auto ref = project->ref();
        ref.channel = "main";
        auto ret = repo.checkoutAll(ref, "", targetPath);
        if (!ret.has_value()) {
            return NewError(-1, "checkout app files failed");
        }

        // checkout runtime
        targetPath =
          BuilderConfig::instance()->layerPath({ project->runtimeRef().toLocalRefString() });
        linglong::util::ensureDir(targetPath);

        auto remoteRuntimeRef = package::Ref(BuilderConfig::instance()->remoteRepoName,
                                             "linglong",
                                             project->runtimeRef().appId,
                                             project->runtimeRef().version,
                                             project->runtimeRef().arch,
                                             "");

        auto latestRuntimeRef = repo.remoteLatestRef(remoteRuntimeRef);
        ret = repo.checkoutAll(*latestRuntimeRef, "", targetPath);
        if (!ret.has_value()) {
            return NewError(-1, "checkout runtime files failed");
        }

        // 获取环境变量
        QStringList userEnvList = utils::command::getUserEnv(utils::command::envList);

        auto app = runtime::App::load(&repo, project->ref(), BuilderConfig::instance()->getExec());
        if (nullptr == app) {
            return NewError(-1, "load App::load failed");
        }

        app->saveUserEnvList(userEnvList);
        app->start();
    } catch (std::exception &e) {
        return NewError(-1, "failed to parse linglong.yaml. " + QString(e.what()));
    }

    return Success();
}

} // namespace linglong::builder
