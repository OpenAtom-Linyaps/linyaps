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
#include "linglong/runtime/container.h"
#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/runner.h"
#include "linglong/util/uuid.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/command/ocppi-helper.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/xdg/desktop_entry.h"
#include "nlohmann/json_fwd.hpp"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/ContainerID.hpp"
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
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>

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

linglong::utils::error::Result<void> LinglongBuilder::buildStageCommitBuildOutput(
  Project *project, const QString &upperdir, const QString &workdir)
{
    LINGLONG_TRACE("commit build output");

    auto output = project->config().cacheDevelLayer("files");
    auto lowerDir = project->config().cacheAbsoluteFilePath({ "overlayfs", "lower" });
    // if combine runtime, lower is ${PROJECT_CACHE}/runtime/files
    if (PackageKindRuntime == project->package->kind) {
        lowerDir = project->config().cacheAbsoluteFilePath({ "runtime", "files" });
    }

    util::ensureDir(lowerDir);
    util::ensureDir(upperdir);
    util::ensureDir(workdir);
    util::ensureDir(output);
    util::ensureDir(project->config().cacheRuntimeLayer("files"));

    QString program = "fuse-overlayfs";
    QStringList arguments = {
        "-o", "upperdir=" + upperdir, "-o",   "workdir=" + workdir,
        "-o", "lowerdir=" + lowerDir, output,
    };
    qDebug() << "run" << program << arguments.join(" ");
    auto execRet = utils::command::Exec(program, arguments);
    if (!execRet) {
        return LINGLONG_ERR(execRet);
    }

    auto _ = qScopeGuard([=] {
        qDebug() << "umount fuse overlayfs";
        auto ret = utils::command::Exec("umount", { "--lazy", output });
        if (!ret) {
            qCritical() << "Failed to umount" << ret.error();
        }
    });

    // split files which in devel layer to runtime layer
    auto splitFile = [](Project *project) -> linglong::utils::error::Result<void> {
        LINGLONG_TRACE("split layers file");
        // get install file rule
        QStringList installRules;
        QStringList installedFiles;

        // if ${PROJECT_ROOT}/${appid}.install is not exist, copy all files
        const auto installRulePath = QStringList{ project->config().rootPath(),
                                                  QString("%1.install").arg(project->package->id) }
                                       .join(QDir::separator());
        if (!QFileInfo(installRulePath).exists()) {
            installRules << QStringList{ project->config().targetInstallPath(""), "*" }.join(
              QDir::separator());
        } else {
            QFile configFile(installRulePath);
            if (!configFile.open(QIODevice::ReadOnly)) {
                return LINGLONG_ERR(configFile.errorString() + " " + configFile.fileName(),
                                    configFile.error());
            }
            installRules.append(QString(configFile.readAll()).split('\n'));
            // remove empty or duplicate lines
            installRules.removeAll("");
            installRules.removeDuplicates();
        }

        const QString src = project->config().cacheDevelLayer("files");
        const QString dest = project->config().cacheRuntimeLayer("files");
        const QString prefix = project->config().targetInstallPath("");
        for (auto rule : installRules) {
            // convert prefix in container to real path in host
            // /opt/apps/${appid} to $PROJECT_ROOT/.../files
            rule.replace(prefix, src);
            // reverse files in src
            QDirIterator it(src,
                            QDir::AllEntries | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();

                QRegExp rx(rule, Qt::CaseInsensitive, QRegExp::Wildcard);
                if (!rx.exactMatch(it.fileInfo().absoluteFilePath()))
                    continue;

                const QString dstPath = it.fileInfo().absoluteFilePath().replace(src, dest);
                if (it.fileInfo().isDir()) {
                    qDebug() << "matched dir" << it.fileInfo().absoluteFilePath();
                    QDir().mkpath(dstPath);
                    installedFiles.append(dstPath);
                    continue;
                }
                if (it.fileInfo().isSymLink()) {
                    qDebug() << "matched symlinks" << it.fileInfo().absoluteFilePath();
                    char buf[PATH_MAX];
                    auto size = readlink(it.fileInfo().filePath().toStdString().c_str(),
                                         buf,
                                         sizeof(buf) - 1);
                    if (size == -1) {
                        qCritical() << "readlink failed! " << it.fileInfo().filePath();
                        continue;
                    }

                    buf[size] = '\0';
                    QFileInfo originFile(it.fileInfo().readLink());
                    QString newLinkFile = dstPath;

                    if (QString(buf).startsWith("/")) {
                        if (!QFile::link(it.fileInfo().readLink(), newLinkFile))
                            return LINGLONG_ERR("link file failed, absolute path", -1);
                    } else {
                        // caculator the relative path
                        QDir linkFileDir(it.fileInfo().dir());
                        QString relativePath = linkFileDir.relativeFilePath(originFile.path());
                        auto newOriginFile = relativePath.endsWith("/")
                          ? relativePath + originFile.fileName()
                          : relativePath + "/" + originFile.fileName();
                        qDebug() << "link" << newOriginFile << "to" << newLinkFile;
                        if (!QFile::link(newOriginFile, newLinkFile))
                            return LINGLONG_ERR("link file failed, relative path", -1);
                    }
                    installedFiles.append(dstPath);
                    continue;
                }
                if (it.fileInfo().isFile()) {
                    qDebug() << "matched file" << it.fileInfo().absoluteFilePath();
                    QDir().mkpath(it.fileInfo().path().replace(src, dest));
                    QFile file(it.fileInfo().absoluteFilePath());
                    if (!file.copy(dstPath))
                        return LINGLONG_ERR("copy file failed: " + file.errorString(),
                                            file.error());
                    installedFiles.append(dstPath);
                }
            }
        }

        installedFiles.replaceInStrings(dest, prefix);
        // save all installed file path to ${appid}.install
        const auto installRuleSavePath =
          QStringList{ project->config().cacheDevelLayer(""),
                       QString("%1.install").arg(project->package->id) }
            .join(QDir::separator());
        const auto installRuleDumpPath =
          QStringList{ project->config().cacheRuntimeLayer(""),
                       QString("%1.install").arg(project->package->id) }
            .join(QDir::separator());
        QFile configFile(installRuleSavePath);
        if (!configFile.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR(configFile.errorString() + " " + configFile.fileName(),
                                configFile.error());
        }
        if (configFile.write(installedFiles.join('\n').toLocal8Bit()) < 0) {
            return LINGLONG_ERR(configFile.errorString(), configFile.error());
        }
        configFile.close();

        // dump ${appid}.install to runtime layer
        if (!QFile::copy(installRuleSavePath, installRuleDumpPath))
            return LINGLONG_ERR("dump install rule failed", -1);

        return LINGLONG_OK;
    };

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
        info->size = linglong::util::sizeOfDir(project->config().cacheRuntimeLayer(""));
        QFile infoFile(project->config().cacheRuntimeLayer("info.json"));
        if (!infoFile.open(QIODevice::WriteOnly)) {
            return NewError(infoFile.error(), infoFile.errorString() + " " + infoFile.fileName());
        }

        // FIXME(black_desk): handle error
        if (infoFile.write(std::get<0>(util::toJSON(info))) < 0) {
            return NewError(infoFile.error(), infoFile.errorString());
        }
        infoFile.close();

        info->module = "devel";
        info->size = linglong::util::sizeOfDir(project->config().cacheDevelLayer(""));
        QFile develInfoFile(project->config().cacheDevelLayer("info.json"));
        if (!develInfoFile.open(QIODevice::WriteOnly)) {
            return NewError(develInfoFile.error(),
                            develInfoFile.errorString() + " " + develInfoFile.fileName());
        }
        if (develInfoFile.write(std::get<0>(util::toJSON(info))) < 0) {
            return NewError(develInfoFile.error(), develInfoFile.errorString());
        }
        develInfoFile.close();
        QFile sourceConfigFile(project->configFilePath);
        if (!sourceConfigFile.copy(project->config().cacheDevelLayer("linglong.yaml"))) {
            return NewError(sourceConfigFile.error(), sourceConfigFile.errorString());
        }

        return Success();
    };

    auto splitStatus = splitFile(project);
    if (!splitStatus.has_value()) {
        return LINGLONG_ERR(splitStatus);
    }

    // link devel-install/files/share to devel-install/entries/share/
    linglong::util::linkDirFiles(project->config().cacheDevelLayer("files/share"),
                                 project->config().cacheDevelLayer("entries"));
    // link devel-install/files/lib/systemd to devel-install/entries/systemd
    linglong::util::linkDirFiles(project->config().cacheDevelLayer("files/lib/systemd/user"),
                                 project->config().cacheDevelLayer("entries/systemd/user"));

    linglong::util::linkDirFiles(project->config().cacheRuntimeLayer("files/share"),
                                 project->config().cacheRuntimeLayer("entries"));
    // link runtime-install/files/lib/systemd to runtime-install/entries/systemd
    linglong::util::linkDirFiles(project->config().cacheRuntimeLayer("files/lib/systemd/user"),
                                 project->config().cacheRuntimeLayer("entries/systemd/user"));

    auto err = createInfo(project);
    err = createInfo(project);
    if (err) {
        return LINGLONG_ERR("create info " + err.message(), err.code());
    }
    auto refRuntime = project->refWithModule("runtime");
    refRuntime.channel = "main";
    qDebug() << "importDirectory " << project->config().cacheRuntimeLayer("");
    auto ret = repo.importDirectory(refRuntime, project->config().cacheRuntimeLayer(""));

    if (!ret) {
        return LINGLONG_ERR("import runtime", ret);
    }

    auto refDevel = project->refWithModule("devel");
    refDevel.channel = "main";
    qDebug() << "importDirectory " << project->config().cacheDevelLayer("");
    ret = repo.importDirectory(refDevel, project->config().cacheDevelLayer(""));
    if (!ret.has_value()) {
        return LINGLONG_ERR("import devel", ret);
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
linglong::utils::error::Result<void> LinglongBuilder::buildStageRunContainer(
  QDir workdir, ocppi::cli::CLI &cli, ocppi::runtime::config::types::Config &r)
{
    LINGLONG_TRACE("run container");

    // 写容器配置文件
    auto data = toJSON(r).dump();
    QFile f(workdir.filePath("config.json"));
    qDebug() << "container config file" << f.fileName();
    if (!f.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(f);
    }
    if (!f.write(QByteArray::fromStdString(data))) {
        return LINGLONG_ERR(f);
    }
    f.close();

    // 运行容器
    auto id = util::genUuid().toStdString();
    auto path = std::filesystem::path(workdir.path().toStdString());
    auto ret = cli.run(id.c_str(), path);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    return LINGLONG_OK;
}

utils::error::Result<void> LinglongBuilder::appimageConvert(const QStringList &templateArgs)
{
    LINGLONG_TRACE("convert appimage");

    const auto file = templateArgs.at(0);
    const auto url = templateArgs.at(1);
    const auto hash = templateArgs.at(2);
    const auto id = templateArgs.at(3);
    const auto name = templateArgs.at(4);
    const auto version = templateArgs.at(5);
    const auto description = templateArgs.at(6);
    const auto scriptName = templateArgs.at(7);

    auto projectPath = QStringList{ QDir::currentPath(), name }.join(QDir::separator());
    auto configFilePath = QStringList{ projectPath, "linglong.yaml" }.join(QDir::separator());
    auto templateFilePath =
      QStringList{ BuilderConfig::instance()->templatePath(), "appimage.yaml" }.join(
        QDir::separator());

    if (!QFileInfo::exists(templateFilePath)) {
        return LINGLONG_ERR("linglong installation broken: template file appimage.yaml not found");
    }

    auto ret = QDir().mkpath(projectPath);
    if (!ret) {
        return LINGLONG_ERR(QString("project path %1 already exists").arg(projectPath));
    }

    // copy appimage file to project path
    if (!file.isEmpty()) {
        QFileInfo fileInfo(file);
        const auto &sourcefilePath = fileInfo.absoluteFilePath();
        const auto &destinationFilePath = QDir(projectPath).filePath(fileInfo.fileName());

        if (!QFileInfo::exists(sourcefilePath)) {
            return LINGLONG_ERR(QString("appimage file %1 not found").arg(sourcefilePath));
        }

        QFile::copy(sourcefilePath, destinationFilePath);
    }

    if (QFileInfo::exists(templateFilePath)) {
        QFile::copy(templateFilePath, configFilePath);
    } else {
        QFile::copy(":/appimage.yaml", configFilePath);
    }

    // config linglong.yaml before build if necessary
    if (!QFileInfo::exists(configFilePath)) {
        return LINGLONG_ERR(
          "linglong.yaml not found, ll-builder should run in the root directory of the project.");
    }

    auto [project, err] =
      linglong::util::fromYAML<QSharedPointer<linglong::builder::Project>>(configFilePath);
    if (err) {
        return LINGLONG_ERR(err.message(), err.code());
    }

    auto node = YAML::LoadFile(configFilePath.toStdString());

    node["package"]["id"] = id.isEmpty() ? project->package->id.toStdString() : id.toStdString();
    node["package"]["name"] =
      name.isEmpty() ? project->package->name.toStdString() : name.toStdString();
    node["package"]["version"] =
      version.isEmpty() ? project->package->version.toStdString() : version.toStdString();
    node["package"]["description"] = description.isEmpty()
      ? project->package->description.toStdString()
      : description.toStdString();

    if (!(url.isEmpty() && hash.isEmpty())) {
        node["sources"][0]["kind"] =
          url.isEmpty() ? project->sources.first()->kind.toStdString() : "file";
        node["sources"][0]["url"] = url.isEmpty() ? "" : url.toStdString();
        node["sources"][0]["digest"] = url.isEmpty() ? "" : hash.toStdString();
    }

    // fixme: use qt file stream
    std::ofstream fout(configFilePath.toStdString());
    fout << node;
    fout.close();

    // if user not specified -o option, export linglong .layer(.uab) directly, or generate
    // linglong.yaml and convert.sh
    if (scriptName.isEmpty()) {
        linglong::builder::BuilderConfig::instance()->setProjectRoot(projectPath);

        auto buildRet = build();
        if (!buildRet) {
            return LINGLONG_ERR(buildRet);
        }
        err = exportLayer(projectPath);

        // delete the generated temporary file, only keep .layer(.uab) files
        auto ret = utils::command::Exec(
          "bash",
          QStringList() << "-c"
                        << "find . -maxdepth 1 -not -regex '.*\\.\\|.*\\.layer\\|.*\\.uab' "
                           "-exec basename {} -print0 \\;  | xargs rm -r");
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    } else {
        auto scriptFilePath = QStringList{ projectPath, scriptName }.join(QDir::separator());

        // copy convert.sh to project path
        if (!QFile::copy(":/convert.sh", scriptFilePath)) {
            return LINGLONG_ERR("create convert.sh failed.");
        }

        // set execution permissions to convert.sh
        QFile scriptFile(scriptFilePath);
        if (!scriptFile.setPermissions(QFile::ReadOwner | QFile::ExeOwner)) {
            return LINGLONG_ERR("set convert.sh permission", scriptFile);
        }
    }

    return LINGLONG_OK;
}

util::Error LinglongBuilder::create(const QString &projectName)
{
    auto projectPath = QStringList{ QDir::currentPath(), projectName }.join(QDir::separator());
    auto configFilePath = QStringList{ projectPath, "linglong.yaml" }.join(QDir::separator());
    auto templateFilePath =
      QStringList{ BuilderConfig::instance()->templatePath(), "example.yaml" }.join(
        QDir::separator());

    auto ret = QDir().mkpath(projectPath);
    if (!ret) {
        return NewError(-1, "project already exists");
    }

    if (QFileInfo::exists(templateFilePath)) {
        QFile::copy(templateFilePath, configFilePath);
    } else {
        QFile::copy(":/example.yaml", configFilePath);
    }

    return Success();
}

util::Error LinglongBuilder::track()
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

    for (const auto source : project->sources) {
        if ("git" == source->kind) {
            auto gitOutput = QSharedPointer<QByteArray>::create();
            QString latestCommit;
            // default git ref is HEAD
            auto gitRef = source->version.isEmpty() ? "HEAD" : source->version;
            auto err = util::Exec("git", { "ls-remote", source->url, gitRef }, -1, gitOutput);
            if (!err) {
                latestCommit = QString::fromLocal8Bit(*gitOutput).trimmed().split("\t").first();
                qDebug() << latestCommit;

                if (source->commit == latestCommit) {
                    printer.printMessage("current commit is the latest, nothing to update");
                } else {
                    std::ofstream fout(projectConfigPath.toStdString());
                    auto result = util::toYAML(project);
                    fout << std::get<0>(result).toStdString();
                    printer.printMessage("update commit to:" + latestCommit);
                }
            }
        }
    }

    return Success();
}

utils::error::Result<void> LinglongBuilder::buildStageClean(const Project &project)
{
    // initialize some directories
    util::removeDir(project.config().cacheRuntimePath({}));
    util::removeDir(project.config().cacheRuntimeLayer(""));
    util::removeDir(project.config().cacheDevelLayer(""));
    util::removeDir(project.config().cacheAbsoluteFilePath({ "overlayfs" }));
    util::ensureDir(project.config().cacheRuntimeLayer(""));
    util::ensureDir(project.config().cacheDevelLayer(""));
    util::ensureDir(project.config().cacheAbsoluteFilePath(
      { "overlayfs", "up", project.config().targetInstallPath("") }));
    return LINGLONG_OK;
}

utils::error::Result<QString> LinglongBuilder::buildStageDepend(Project *project)
{
    LINGLONG_TRACE("handle depends");

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
        DependFetcher df(runtimeDepend, repo, printer, project);
        auto err = df.fetch("", project->config().cacheRuntimePath(""));
        if (err) {
            return LINGLONG_ERR(err.message(), err.code());
        }

        if (!project->base) {
            QFile infoFile(project->config().cacheRuntimePath("info.json"));
            if (!infoFile.open(QIODevice::ReadOnly)) {
                return LINGLONG_ERR("open info.json", infoFile);
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
    DependFetcher baseFetcher(baseDepend, repo, printer, project);
    // TODO: the base filesystem hierarchy is just an debian now. we should change it
    hostBasePath = BuilderConfig::instance()->layerPath({ baseRef.toLocalRefString(), "" });
    auto err = baseFetcher.fetch("", hostBasePath);
    if (err) {
        return LINGLONG_ERR(err.message(), err.code());
    }

    // depends fetch
    for (auto const &depend : project->depends) {
        DependFetcher df(depend, repo, printer, project);
        err = df.fetch("files", project->config().cacheRuntimePath("files"));
        if (err) {
            return LINGLONG_ERR(err.message(), err.code());
        }
    }
    return hostBasePath;
}

linglong::utils::error::Result<ocppi::runtime::config::types::Config>
LinglongBuilder::buildStageConfigInit()
{
    LINGLONG_TRACE("init oci config");

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
    if (!r) {
        printer.printMessage("builtin OCI configuration template is invalid.");
        return LINGLONG_ERR(r);
    }
    return *r;
}

linglong::utils::error::Result<void>
LinglongBuilder::buildStageRuntime(ocppi::runtime::config::types::Config &r,
                                   const Project &project,
                                   const QString &overlayLowerDir,
                                   const QString &overlayUpperDir,
                                   const QString &overlayWorkDir,
                                   const QString &overlayMergeDir)
{
    LINGLONG_TRACE("handle runtime");

    util::ensureDir(overlayLowerDir);
    util::ensureDir(overlayUpperDir);
    util::ensureDir(overlayWorkDir);
    util::ensureDir(overlayMergeDir);
    if (project.package->kind == PackageKindApp) {
        // app 会安装到到 /opt/apps/$appid/files，不和 runtime 的内容混合
        // 所以挂载只读的 /runtime
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = overlayLowerDir.toStdString();
        m.destination = "/runtime";
        m.options = { "rbind", "ro", "nosuid", "nodev" };
        r.mounts->push_back(m);
        // 挂载 overlayfs/up/opt/apps/$appid/files 到 /opt/apps/$appid/files 目录
        // 在构建结束后，会从 overlayfs/up 获取构建产物
        // 所以在不使用fuse时也挂载 overlayfs/up 作为安装目录
        m.source = overlayUpperDir.toStdString();
        m.destination = "/opt/apps/" + project.ref().appId.toStdString() + "/files";
        m.options = { "rbind", "rw", "nosuid", "nodev" };
        r.mounts->push_back(m);
    } else {
        // lib 会安装到 /runtime，和 runtime 的内容混合在一起,
        // 所以使用 fuse-overlayfs 挂载可写的 /runtime

        //  使用命令挂载fuse-overlayfs runtime
        QString program = "fuse-overlayfs";
        QStringList arguments = {
            "-o",
            "upperdir=" + overlayUpperDir,
            "-o",
            "workdir=" + overlayWorkDir,
            "-o",
            "lowerdir=" + overlayLowerDir,
            overlayMergeDir,
        };
        qDebug() << "run" << program << arguments.join(" ");
        auto ret = utils::command::Exec(program, arguments);
        if (!ret) {
            return LINGLONG_ERR("mount runtime", ret);
        }
        // 在annotations中添加预挂载的注释，便于调试重现
        utils::command::AddAnnotation(r,
                                      utils::command::AnnotationKey::MountRuntimeComments,
                                      program + " " + arguments.join(" "));
        // 使用容器hooks卸载fuse-overlayfs runtime
        ocppi::runtime::config::types::Hook umountRuntimeHook;
        umountRuntimeHook.args = { "umount", "--lazy", overlayMergeDir.toStdString() };
        umountRuntimeHook.path = "/usr/bin/umount";
        if (!r.hooks.has_value()) {
            r.hooks = std::make_optional(ocppi::runtime::config::types::Hooks{});
        }
        if (r.hooks->poststop.has_value()) {
            r.hooks->poststop->push_back(umountRuntimeHook);
        } else {
            r.hooks->poststop = { umountRuntimeHook };
        }
        QStringList opt = { "rbind", "rw", "nosuid", "nodev" };
        utils::command::AddMount(r, overlayMergeDir, "/runtime", opt);
    }
    return LINGLONG_OK;
}

utils::error::Result<void>
LinglongBuilder::buildStageSource(ocppi::runtime::config::types::Config &r, Project *project)
{
    LINGLONG_TRACE("process source");

    printer.printMessage("[Processing Source]");
    QSharedPointer<SourceFetcher> sf;
    for (const auto source : project->sources) {
        sf.reset(new SourceFetcher(source, printer, project));
        if (source) {
            auto err = sf->fetch();
            if (err) {
                return LINGLONG_ERR("fetch source failed");
            }
        }
    }
    // 挂载tmp
    ocppi::runtime::config::types::Mount m;
    m.type = "tmpfs";
    m.source = "tmp";
    m.destination = "/tmp";
    m.options = { "nosuid", "strictatime", "mode=777" };
    r.mounts->push_back(m);
    // 挂载构建文件
    auto containerSourcePath = "/source";
    QList<QPair<QString, QString>> mountMap = {
        // 源码目录
        { sf->sourceRoot(), containerSourcePath },
        // 构建脚本
        { project->buildScriptPath(), BuildScriptPath },
    };
    for (const auto &pair : mountMap) {
        ocppi::runtime::config::types::Mount m;
        m.type = "bind";
        m.source = pair.first.toStdString();
        m.destination = pair.second.toStdString();
        m.options = { "rbind", "nosuid", "nodev" };
        r.mounts->push_back(m);
    }

    if (!BuilderConfig::instance()->getExec().empty()) {
        r.process->terminal = true;
    }
    r.process->args = { "/bin/bash", "-e", BuildScriptPath };
    r.process->cwd = containerSourcePath;
    r.process->user = ocppi::runtime::config::types::User();
    r.process->user->uid = getuid();
    r.process->user->gid = getgid();
    return LINGLONG_OK;
}

utils::error::Result<void>
LinglongBuilder::buildStageEnvrionment(ocppi::runtime::config::types::Config &r,
                                       const Project &project,
                                       const BuilderConfig &buildConfig)
{
    // 兼容旧版本的自动使用当前proxy，后面考虑弃用这些
    auto envList = QStringList{ "HTTP_PROXY", "HTTPS_PROXY", "NO_PROXY", "ALL_PROXY", "FTP_PROXY" };
    for (auto key : envList) {
        auto val = QProcessEnvironment::systemEnvironment().value(key);
        if (val.isEmpty()) {
            key = key.toLower();
            val = QProcessEnvironment::systemEnvironment().value(key);
        }
        if (!val.isEmpty()) {
            r.process->env->push_back(QString("%1=%2").arg(key).arg(val).toStdString());
        }
    }
    // 一些默认的环境变量
    r.process->env->push_back("PREFIX=" + project.config().targetInstallPath("").toStdString());
    r.process->env->push_back(
      "PATH=/runtime/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin");

    if (project.config().targetArch() == "x86_64") {
        r.process->env->push_back("PKG_CONFIG_PATH=/runtime/lib/x86_64-linux-gnu/pkgconfig");
        r.process->env->push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
        r.process->env->push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/x86_64-linux-gnu");
    } else if (project.config().targetArch() == "arm64") {
        r.process->env->push_back("PKG_CONFIG_PATH=/runtime/lib/aarch64-linux-gnu/pkgconfig");
        r.process->env->push_back("LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
        r.process->env->push_back("LD_LIBRARY_PATH=/runtime/lib:/runtime/lib/aarch64-linux-gnu");
    }
    // 通过命令行设置的环境变量
    for (auto env : buildConfig.getBuildEnv()) {
        r.process->env->push_back(env.toStdString());
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void>
LinglongBuilder::buildStageIDMapping(ocppi::runtime::config::types::Config &r)
{
    // 映射uid
    QList<QList<int64_t>> uidMaps = {
        { getuid(), getuid(), 1 },
    };
    for (auto const &uidMap : uidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = uidMap.value(0);
        idMap.containerID = uidMap.value(1);
        idMap.size = uidMap.value(2);
        if (r.linux_->uidMappings.has_value()) {
            r.linux_->uidMappings->push_back(idMap);
        } else {
            r.linux_->uidMappings = { idMap };
        }
    }
    // 映射gid
    QList<QList<quint64>> gidMaps = {
        { getgid(), getuid(), 1 },
    };
    for (auto const &gidMap : gidMaps) {
        ocppi::runtime::config::types::IdMapping idMap;
        idMap.hostID = gidMap.value(0);
        idMap.containerID = gidMap.value(1);
        idMap.size = gidMap.value(2);
        if (r.linux_->gidMappings.has_value()) {
            r.linux_->gidMappings->push_back(idMap);
        } else {
            r.linux_->gidMappings = { idMap };
        }
    }
    return LINGLONG_OK;
}

linglong::utils::error::Result<void> LinglongBuilder::buildStageRootfs(
  ocppi::runtime::config::types::Config &r, const QDir &workdir, const QString &hostBasePath)
{
    LINGLONG_TRACE("process rootfs");

    // 初始化rootfs目录
    workdir.mkdir("rootfs");
    workdir.mkdir("upper");
    workdir.mkdir("work");
    // 使用命令挂载 overlayfs rootfs
    QString program = "fuse-overlayfs";
    QStringList arguments = {
        "-o",
        "upperdir=" + workdir.filePath("upper"),
        "-o",
        "workdir=" + workdir.filePath("work"),
        "-o",
        "lowerdir=" + QDir(hostBasePath).filePath("files"),
        workdir.filePath("rootfs"),
    };
    qDebug() << "run" << program << arguments.join(" ") << r.hooks->poststop.has_value();
    auto mountRootRet = utils::command::Exec(program, arguments);
    if (!mountRootRet) {
        return LINGLONG_ERR(mountRootRet);
    }
    //    在annotations中添加预挂载的注释，便于调试重现
    utils::command::AddAnnotation(r,
                                  utils::command::AnnotationKey::MountRootfsComments,
                                  program + " " + arguments.join(" "));
    // 使用容器hooks卸载overlayfs rootfs
    ocppi::runtime::config::types::Hook umountRootfsHook;
    umountRootfsHook.args = { "umount", "--lazy", workdir.filePath("rootfs").toStdString() };
    umountRootfsHook.path = "/usr/bin/umount";
    if (!r.hooks.has_value()) {
        r.hooks = std::make_optional(ocppi::runtime::config::types::Hooks{});
    }
    if (r.hooks->poststop.has_value()) {
        r.hooks->poststop->push_back(umountRootfsHook);
    } else {
        r.hooks->poststop = { umountRootfsHook };
    }

    // 使用rootfs做容器根目录
    r.root->readonly = true;
    r.root->path = workdir.filePath("rootfs").toStdString();
    return LINGLONG_OK;
}

linglong::utils::error::Result<QSharedPointer<Project>> LinglongBuilder::buildStageProjectInit()
{
    LINGLONG_TRACE("init project");

    linglong::util::Error err;
    auto projectConfigPath =
      QStringList{ BuilderConfig::instance()->getProjectRoot(), "linglong.yaml" }.join("/");

    if (!QFileInfo::exists(projectConfigPath)) {
        printer.printMessage("ll-builder should run in the root directory of the linglong project");
        return LINGLONG_ERR("linglong.yaml not found");
    }
    auto [project, err2] = util::fromYAML<QSharedPointer<Project>>(projectConfigPath);
    if (err2) {
        return LINGLONG_ERR("load project yaml: " + err2.message(), err2.code());
    }
    if (!project->package || project->package->kind.isEmpty()) {
        return LINGLONG_ERR("unknown package kind");
    }

    project->generateBuildScript();
    project->configFilePath = projectConfigPath;

    linglong::builder::BuilderConfig::instance()->setProjectName(project->package->id);

    return project;
}

linglong::utils::error::Result<void> LinglongBuilder::build()
{
    LINGLONG_TRACE("build");

    auto projectRet = buildStageProjectInit();
    if (!projectRet) {
        return LINGLONG_ERR(projectRet);
    }
    auto configRet = buildStageConfigInit();
    if (!configRet) {
        return LINGLONG_ERR(configRet);
    }
    auto project = *projectRet;
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

    auto voidRet = buildStageClean(*project);
    if (!voidRet) {
        return LINGLONG_ERR("clean build", voidRet);
    }
    auto basePathRet = buildStageDepend(project.get());
    if (!basePathRet) {
        return LINGLONG_ERR("processing depend", basePathRet);
    }
    auto hostBasePath = *basePathRet;
    auto containerConfig = *configRet;
    // 初始化 overlayfs 目录
    QDir overlayDir(project->config().cacheAbsoluteFilePath({ "overlayfs" }));
    // overlayLowerDir 底层目录里面存放项目使用的runtime和depends文件的混合
    auto overlayLowerDir = project->config().cacheAbsoluteFilePath({ "runtime", "files" });
    // overlayUpperDir 上层目录用于存储新增和变动的文件
    // 项目的depends也会提前放到这个目录中，因为玲珑会将项目依赖和项目打包到一起
    auto overlayUpperDir = overlayDir.filePath("up/") + project->config().targetInstallPath("");
    // overlayWorkDir 是overlay的内部工作目录
    auto overlayWorkDir = overlayDir.filePath("wk");
    // point 目录是 overlay 的挂载点，会映射到容器中
    auto overlayPointDir = overlayDir.filePath("point");

    // 挂载runtime
    voidRet = buildStageRuntime(containerConfig,
                                *project,
                                overlayLowerDir,
                                overlayUpperDir,
                                overlayWorkDir,
                                overlayPointDir);
    if (!voidRet) {
        return LINGLONG_ERR(voidRet);
    }
    // 挂载源码并配置构建脚本
    voidRet = buildStageSource(containerConfig, project.get());
    if (!voidRet) {
        return LINGLONG_ERR(voidRet);
    }
    // 配置环境变量
    voidRet = buildStageEnvrionment(containerConfig, *project, *BuilderConfig::instance());
    if (!voidRet.has_value()) {
        return LINGLONG_ERR(voidRet);
    }
    // 配置uid和gid映射
    voidRet = buildStageIDMapping(containerConfig);
    if (!voidRet) {
        return LINGLONG_ERR(voidRet);
    }
    // 配置容器rootfs
    QTemporaryDir tmpDir;
    tmpDir.setAutoRemove(false);
    voidRet = buildStageRootfs(containerConfig, tmpDir.path(), hostBasePath);
    if (!voidRet) {
        return LINGLONG_ERR(voidRet);
    }
    // 运行容器
    voidRet = buildStageRunContainer(tmpDir.path(), ociCLI, containerConfig);
    if (!voidRet) {
        // 为避免容器启动失败，hook脚本未执行，手动再执行一次 umount
        if (project->package->kind != PackageKindApp) {
            qDebug() << "umount runtime";
            utils::command::Exec("umount", { "--lazy", overlayPointDir });
        }
        qDebug() << "umount rootfs";
        utils::command::Exec("umount",
                             { "--lazy", QString::fromStdString(containerConfig.root->path) });
        return LINGLONG_ERR(voidRet);
    }
    // 提交构建结果
    voidRet = buildStageCommitBuildOutput(project.get(), overlayUpperDir, overlayPointDir);
    if (!voidRet) {
        return LINGLONG_ERR(voidRet);
    }

    printer.printMessage(QString("Build %1 success").arg(project->package->id));
    return LINGLONG_OK;
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
