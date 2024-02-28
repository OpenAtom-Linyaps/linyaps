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
#include "linglong/util/uuid.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/std_helper/qdebug_helper.h"
#include "linglong/utils/xdg/desktop_entry.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/ConfigLoader.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"
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

LinglongBuilder::LinglongBuilder(repo::OSTreeRepo &ostree,
                                 cli::Printer &p,
                                 ocppi::cli::CLI &cli,
                                 service::AppManager &appManager)
    : repo(ostree)
    , printer(p)
    , ociCLI(cli)
    , appManager(appManager)
{
}

linglong::utils::error::Result<void> LinglongBuilder::commitBuildOutput(Project *project,
                                                                        const QString &upperdir,
                                                                        const QString &workdir)
{
    auto output = project->config().cacheInstallPath("files");
    auto lowerDir = project->config().cacheAbsoluteFilePath({ "overlayfs", "lower" });
    // if combine runtime, lower is ${PROJECT_CACHE}/runtime/files
    if (PackageKindRuntime == project->package->kind) {
        lowerDir = project->config().cacheAbsoluteFilePath({ "runtime", "files" });
    }

    util::ensureDir(lowerDir);
    util::ensureDir(upperdir);
    util::ensureDir(workdir);
    util::ensureDir(output);

    QProcess fuseOverlayfs;
    fuseOverlayfs.setProgram("fuse-overlayfs");
    fuseOverlayfs.setArguments({
      "-o",
      "upperdir=" + upperdir,
      "-o",
      "workdir=" + workdir,
      "-o",
      "lowerdir=" + lowerDir,
      output,
    });

    qDebug() << "run" << fuseOverlayfs.program() << fuseOverlayfs.arguments().join(" ");

    fuseOverlayfs.start();
    if (!fuseOverlayfs.waitForFinished()) {
        return LINGLONG_ERR(-1, "exec fuse-overlayfs failed!" + fuseOverlayfs.errorString());
    }
    auto _ = qScopeGuard([=] {
        qDebug() << "umount fuse overlayfs";
        QProcess::execute("umount", { "--lazy", output });
    });

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
        return LINGLONG_ERR(err.code(), "modify config file: " + err.message());
    }

    // modify context-menus file
    auto contextFilePath =
      QStringList{ output, "share/applications/context-menus" }.join(QDir::separator());
    auto contextFileSavePath = contextFilePath;
    err = modifyConfigFile(contextFilePath, contextFileSavePath, "*.conf", appId);
    if (err) {
        return LINGLONG_ERR(err.code(), "modify desktop file: " + err.message());
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
        return LINGLONG_ERR(err.code(), "move debug file: " + err.message());
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
        return LINGLONG_ERR(err.code(), "create info " + err.message());
    }
    auto refRuntime = project->refWithModule("runtime");
    refRuntime.channel = "main";
    qDebug() << "importDirectory " << project->config().cacheInstallPath("");
    auto ret = repo.importDirectory(refRuntime, project->config().cacheInstallPath(""));

    if (!ret.has_value()) {
        return LINGLONG_EWRAP("import runtime", ret.error());
    }

    auto refDevel = project->refWithModule("devel");
    refDevel.channel = "main";
    qDebug() << "importDirectory " << project->config().cacheInstallPath("devel-install", "");
    ret = repo.importDirectory(refDevel, project->config().cacheInstallPath("devel-install", ""));
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("import devel", ret.error());
    }
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
        ref.arch = BuilderConfig::instance()->getBuildArch();
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
linglong::utils::error::Result<void> LinglongBuilder::startContainer(
  QString workdir, ocppi::cli::CLI &cli, ocppi::runtime::config::types::Config &r)
{
    QDir dir(workdir);
    // 初始化rootfs目录
    dir.mkdir("rootfs");
    // 在rootfs目录创建一些软链接，会在容器中使用
    for (auto link : QStringList{ "lib", "lib32", "lib64", "libx32", "bin", "sbin" }) {
        QFile::link("usr/" + link, dir.filePath("rootfs/" + link));
    }
    // 使用rootfs做容器根目录
    r.root->readonly = true;
    r.root->path = dir.filePath("rootfs").toStdString();

    // 写容器配置文件
    auto data = toJSON(r).dump();
    QFile f(dir.filePath("config.json"));
    qDebug() << "container config file" << f.fileName();
    if (!f.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(-1, f.errorString());
    }
    if (!f.write(QByteArray::fromStdString(data))) {
        return LINGLONG_ERR(-1, f.errorString());
    }
    f.close();

    // 运行容器
    auto id = util::genUuid().toStdString();
    auto path = std::filesystem::path(dir.path().toStdString());
    auto ret = cli.run(id.c_str(), path);
    if (!ret.has_value()) {
        auto err = LINGLONG_SERR(ret.error());
        qDebug() << "run container failed" << err.value().message();
        return err;
    }
    return LINGLONG_OK;
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
    printer.printMessage(QString("Package Name: %1").arg(project->package->name), 2);
    printer.printMessage(QString("Version: %1").arg(project->package->version), 2);
    printer.printMessage(QString("Package Type: %1").arg(project->package->kind), 2);
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

    // 挂载base目录
    auto baseDirs =
      QDir(hostBasePath + "files").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for (auto entry : baseDirs) {
        auto source = hostBasePath + "files/" + entry;
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = source.toStdString();
        m.destination = "/" + entry.toStdString();
        m.options = { "rbind", "ro", "nosuid", "nodev" };
        r->mounts->push_back(m);
    };

    // 初始化 overlayfs 目录
    QDir overlayDir(project->config().cacheAbsoluteFilePath({ "overlayfs" }));
    // up 目录是 overlay 的 upperdir，依赖会在 depend fetcher 时 checkout 到里面
    auto overlayUpDir = overlayDir.filePath("up/") + project->config().targetInstallPath("");
    // wk 目录是 overlay 的 workdir，lowerdir 是 runtime 目录
    auto overlayWorkDir = overlayDir.filePath("wk");
    // point 目录是 overlay 的挂载目录
    auto overlayPointDir = overlayDir.filePath("point");

    QProcess fuseOverlayfs;
    // lib 会安装到 /runtime 目录中, 所以使用 fuse-overlayfs 挂载可写的 /runtime
    if (project->package->kind == PackageKindLib) {
        // 由于历史原因，lib库的构建产物在存储时没有/runtime路径前缀
        // 所以需要更改updir目录，来保持兼容
        util::ensureDir(overlayUpDir);
        util::ensureDir(overlayWorkDir);
        util::ensureDir(overlayPointDir);

        fuseOverlayfs.setProgram("fuse-overlayfs");
        fuseOverlayfs.setArguments({
          "-o",
          "upperdir=" + overlayUpDir,
          "-o",
          "workdir=" + overlayUpDir,
          "-o",
          "lowerdir=" + project->config().cacheAbsoluteFilePath({ "runtime", "files" }),
          overlayPointDir,
        });
        qDebug() << "run" << fuseOverlayfs.program() << fuseOverlayfs.arguments().join(" ");
        fuseOverlayfs.start();
        if (!fuseOverlayfs.waitForFinished()) {
            return NewError(-1, "exec fuse-overlayfs failed!" + fuseOverlayfs.errorString());
        }

        // 使用容器hook取消挂载runtime
        r->hooks = r->hooks.value_or(ocppi::runtime::config::types::Hooks{});
        ocppi::runtime::config::types::Hook umountRuntimeHook;
        umountRuntimeHook.args = { "umount", "--lazy", overlayPointDir.toStdString() };
        umountRuntimeHook.path = "/usr/bin/umount";
        r->hooks->poststop = { umountRuntimeHook };

        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = overlayPointDir.toStdString();
        m.destination = "/runtime";
        m.options = { "rbind", "rw", "nosuid", "nodev" };
        r->mounts->push_back(m);
    } else {
        // app 会安装到 /opt/apps/$appid/files ，所以挂载只读的/runtime
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = project->config().cacheAbsoluteFilePath({ "runtime", "files" }).toStdString();
        m.destination = "/runtime";
        m.options = { "rbind", "ro", "nosuid", "nodev" };
        r->mounts->push_back(m);
        // 挂载 overlayfs/up/opt/apps/$appid/files 到 /opt/apps/$appid/files 目录
        // 在构建结束后，会从 overlayfs/up 获取构建产物
        // 所以在不使用fuse时也挂载 overlayfs/up 作为安装目录
        m.source = overlayUpDir.toStdString();
        m.destination = "/opt/apps/" + project->ref().appId.toStdString() + "/files";
        m.options = { "rbind", "rw", "nosuid", "nodev" };
        r->mounts->push_back(m);
    }
    auto container = QSharedPointer<Container>(new Container);
    err = container->create("");
    if (err) {
        return WrapError(err, "create container failed");
    }
    // 挂载tmp
    ocppi::runtime::config::types::Mount m;
    m.type = "tmpfs";
    m.source = "tmp";
    m.destination = "/tmp";
    m.options = { "nosuid", "strictatime", "mode=777" };
    r->mounts->push_back(m);
    // 挂载构建文件
    auto containerSourcePath = "/source";
    QList<QPair<QString, QString>> mountMap = {
        // 源码目录
        { sf.sourceRoot(), containerSourcePath },
        // 构建脚本
        { project->buildScriptPath(), BuildScriptPath },
    };
    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        m.options = { "rbind", "nosuid", "nodev" };
        r->mounts->push_back(m);
    }

    if (!BuilderConfig::instance()->getExec().empty()) {
        r->process->terminal = true;
    }
    r->process->args = { "/bin/bash", "-e", BuildScriptPath };
    r->process->cwd = containerSourcePath;

    // quickfix: we should ensure envrionment variables from config
    QString httpsProxy = qgetenv("https_proxy");
    QString httpProxy = qgetenv("http_proxy");
    QString HTTPSProxy = qgetenv("HTTPS_PROXY");
    QString HTTPProxy = qgetenv("HTTP_PROXY");
    QString noProxy = qgetenv("no_proxy");
    QString allProxy = qgetenv("all_proxy");
    QString ftpProxy = qgetenv("ftp_proxy");
    if (!httpsProxy.isEmpty())
        r->process->env->push_back(QString("https_proxy=%1").arg(httpsProxy).toStdString());
    if (!httpProxy.isEmpty())
        r->process->env->push_back(QString("http_proxy=%1").arg(httpProxy).toStdString());
    if (!HTTPSProxy.isEmpty())
        r->process->env->push_back(QString("HTTPS_PROXY=%1").arg(HTTPSProxy).toStdString());
    if (!HTTPProxy.isEmpty())
        r->process->env->push_back(QString("HTTP_PROXT=%1").arg(HTTPProxy).toStdString());
    if (!noProxy.isEmpty())
        r->process->env->push_back(QString("no_proxy=%1").arg(noProxy).toStdString());
    if (!allProxy.isEmpty())
        r->process->env->push_back(QString("all_proxy=%1").arg(allProxy).toStdString());
    if (!ftpProxy.isEmpty())
        r->process->env->push_back(QString("ftp_proxy=%1").arg(ftpProxy).toStdString());

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

    QList<QList<int64_t>> uidMaps = {
        { getuid(), getuid(), 1 },
    };
    for (auto const &uidMap : uidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = uidMap.value(0);
        idMap.containerID = uidMap.value(1);
        idMap.size = uidMap.value(2);
        r->linux_->uidMappings =
          r->linux_->uidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>());
        r->linux_->uidMappings->push_back(idMap);
    }

    QList<QList<quint64>> gidMaps = {
        { getgid(), getuid(), 1 },
    };
    for (auto const &gidMap : gidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = gidMap.value(0);
        idMap.containerID = gidMap.value(1);
        idMap.size = gidMap.value(2);
        r->linux_->gidMappings =
          r->linux_->gidMappings.value_or(std::vector<ocppi::runtime::config::types::IdMapping>());
        r->linux_->gidMappings->push_back(idMap);
    }
    r->process->user = ocppi::runtime::config::types::User();
    r->process->user->uid = getuid();
    r->process->user->gid = getgid();

    QTemporaryDir tmpDir;
    tmpDir.setAutoRemove(false);
    auto startRet = startContainer(tmpDir.path(), ociCLI, *r);
    if (!startRet.has_value()) {
        return NewError(-1, "build task failed in container: " + startRet.error().message());
    }
    if (fuseOverlayfs.Running) {
        fuseOverlayfs.close();
    }
    auto commitRet = commitBuildOutput(project.get(), overlayUpDir, overlayWorkDir);
    if (!commitRet.has_value()) {
        return NewError(commitRet.error().code(),
                        "commit build output: " + commitRet.error().message());
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
        auto defaultRefWithRuntime = package::Ref("",
                                                  "main",
                                                  project->package->id,
                                                  project->package->version,
                                                  project->config().targetArch(),
                                                  "runtime");
        auto defaultRefWithDevel = package::Ref("",
                                                "main",
                                                project->package->id,
                                                project->package->version,
                                                project->config().targetArch(),
                                                "devel");
        auto newRefWithRuntime = package::Ref("",
                                              channel,
                                              project->package->id,
                                              project->package->version,
                                              project->config().targetArch(),
                                              "runtime");
        auto newRefWithDevel = package::Ref("",
                                            channel,
                                            project->package->id,
                                            project->package->version,
                                            project->config().targetArch(),
                                            "devel");
        auto ret = repo.importRef(defaultRefWithRuntime, newRefWithRuntime);
        if (!ret.has_value()) {
            qDebug() << "change channel failed. " << defaultRefWithRuntime.toOSTreeRefLocalString()
                     << newRefWithRuntime.toOSTreeRefLocalString();
            return NewError(ret.error().code(), ret.error().message());
        }

        ret = repo.importRef(defaultRefWithDevel, newRefWithDevel);
        if (!ret.has_value()) {
            qDebug() << "change channel failed. " << defaultRefWithDevel.toOSTreeRefLocalString()
                     << newRefWithDevel.toOSTreeRefLocalString();
            return NewError(ret.error().code(), ret.error().message());
        }
        // push ostree data by ref
        ret = repo.push(newRefWithRuntime);

        if (!ret.has_value()) {
            printer.printMessage(QString("push %1 failed").arg(project->package->id));
            return NewError(ret.error().code(), ret.error().message());
        } else {
            printer.printMessage(QString("push %1 success").arg(project->package->id));
        }

        if (pushWithDevel) {
            auto ret = repo.push(package::Ref(newRefWithDevel));

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
                                           project->config().targetArch(),
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
        if (project->runtime) {
            // checkout runtime
            targetPath =
              BuilderConfig::instance()->layerPath({ project->runtimeRef().toLocalRefString() });
            linglong::util::ensureDir(targetPath);

            auto latestRuntimeRef = repo.localLatestRef(project->runtimeRef());
            if (!latestRuntimeRef.has_value()) {
                return NewError(latestRuntimeRef.error().code(),
                                latestRuntimeRef.error().message());
            }
            ret = repo.checkoutAll(*latestRuntimeRef, "", targetPath);
            if (!ret.has_value()) {
                return NewError(-1, "checkout runtime files failed");
            }
        }

        service::RunParamOption paramOption;
        paramOption.appId = project->ref().appId;
        paramOption.version = project->ref().version;
        paramOption.appEnv = utils::command::getUserEnv(utils::command::envList);
        paramOption.exec = BuilderConfig::instance()->getExec();
        ret = appManager.Run(paramOption);
        if (!ret.has_value()) {
            return NewError(ret.error().code(), ret.error().message());
        }
    } catch (std::exception &e) {
        return NewError(-1, "failed to parse linglong.yaml. " + QString(e.what()));
    }

    return Success();
}

} // namespace linglong::builder
