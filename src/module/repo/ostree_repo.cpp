/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ostree_repo.h"

#include <QProcess>
#include <QDir>

#include "module/package/ref.h"
#include "module/util/runner.h"
#include "module/util/sysinfo.h"
#include "module/util/version.h"

namespace linglong {
namespace repo {

class OSTreeRepoPrivate
{
    OSTreeRepoPrivate(const QString &path, OSTreeRepo *parent)
        : path(path)
        , q_ptr(parent)
    {
        // FIXME: should be repo
        if (QDir(path).exists("/repo/repo")) {
            qCritical() << path;
            ostreePath = path + "/repo/repo";
        } else {
            ostreePath = path + "/repo";
        }
    }

    linglong::util::Error ostreeRun(const QStringList &args, QByteArray *stdout = nullptr)
    {
        QProcess ostree;
        ostree.setProgram("ostree");

        QStringList ostreeArgs = {"-v", "--repo=" + ostreePath};
        ostreeArgs.append(args);
        ostree.setArguments(ostreeArgs);

        QProcess::connect(&ostree, &QProcess::readyReadStandardOutput, [&]() {
            qDebug() << QString::fromLocal8Bit(ostree.readAllStandardOutput());
            if (stdout) {
                *stdout += ostree.readAllStandardOutput();
            }
        });

        QProcess::connect(&ostree, &QProcess::readyReadStandardError,
                          [&]() { qDebug() << QString::fromLocal8Bit(ostree.readAllStandardError()); });

        qDebug() << "start" << ostree.arguments().join(" ");
        ostree.start();
        ostree.waitForFinished(-1);
        qDebug() << ostree.exitStatus() << "with exit code:" << ostree.exitCode();
 
        return NewError(ostree.exitCode(), ostree.errorString());
    }

    QString path;
    QString ostreePath;

    OSTreeRepo *q_ptr;
    Q_DECLARE_PUBLIC(OSTreeRepo);
};

linglong::util::Error OSTreeRepo::importDirectory(const package::Ref &ref, const QString &path)
{
    Q_D(OSTreeRepo);

    auto ret = d->ostreeRun({"commit", "-b", ref.toString(), "--tree=dir=" + path});

    return ret;
}

linglong::util::Error OSTreeRepo::import(const package::Bundle &bundle)
{
    return linglong::util::Error(nullptr, 0, nullptr);
}

linglong::util::Error OSTreeRepo::exportBundle(package::Bundle &bundle)
{
    return linglong::util::Error(nullptr, 0, nullptr);
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::list(const QString &filter)
{
    return std::tuple<linglong::util::Error, QList<package::Ref>>(NewError(), {});
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::query(const QString &filter)
{
    return std::tuple<linglong::util::Error, QList<package::Ref>>(NewError(), {});
}

/*!
 * ostree-upload push --repo=/deepin/linglong/repo/repo
--token="eEQQSuJBRxFcwa0bz2O5u12w/g1cWXKzexcZGH599y2uYbHbC/IBnExe7KJuWhtBx364AJE0TcUiAIzCIYpQzA==" --address=http:
//bg.iceyer.net:18080 org.deepin.music/6.0.1.54/x86_64
 * @param ref
 * @param force
 * @return
 */
linglong::util::Error OSTreeRepo::push(const package::Ref &ref, bool force)
{
    Q_D(OSTreeRepo);

    auto serverUri = qEnvironmentVariable("OSTREE_UPLOAD_SERVER");
    auto serverToken = qEnvironmentVariable("OSTREE_UPLOAD_TOKEN");

    auto success = runner::Runner("ostree-upload", {
                                                       "push",
                                                       "--repo=" + d->ostreePath,
                                                       // FIXME: where is the token, for user home
                                                       "--token=" + serverToken,
                                                       "--address=" + serverUri,
                                                       ref.toString(),
                                                   });

    return (success ? NoError() : NewError(-1, "ostree upload failed"));
}

linglong::util::Error OSTreeRepo::push(const package::Bundle &bundle, bool force)
{
    return linglong::util::Error(nullptr, 0, nullptr);
}

linglong::util::Error OSTreeRepo::pull(const package::Ref &ref, bool force)
{
    Q_D(OSTreeRepo);
    // Fixme: remote name maybe not repo and there should support multiple remote
    return WrapError(d->ostreeRun({"pull", "repo", "--mirror", ref.toString()}));
}

linglong::util::Error OSTreeRepo::pullAll(const package::Ref &ref, bool force)
{
    Q_D(OSTreeRepo);
    // Fixme: remote name maybe not repo and there should support multiple remote
    auto ret = d->ostreeRun({"pull", "repo", "--mirror", QStringList {ref.toString(), "runtime"}.join("/")});
    if (!ret.success()) {
        return NewError(ret);
    }

    ret = d->ostreeRun({"pull", "repo", "--mirror", QStringList {ref.toString(), "devel"}.join("/")});

    return WrapError(ret);
}

linglong::util::Error OSTreeRepo::init(const QString &mode)
{
    Q_D(OSTreeRepo);

    return WrapError(d->ostreeRun({"init", QString("--mode=%1").arg(mode)}));
}

linglong::util::Error OSTreeRepo::remoteAdd(const QString &repoName, const QString &repoUrl)
{
    Q_D(OSTreeRepo);

    return WrapError(d->ostreeRun({"remote", "add", "--no-gpg-verify", repoName, repoUrl}));
}

OSTreeRepo::OSTreeRepo(const QString &path)
    : dd_ptr(new OSTreeRepoPrivate(path, this))
{
}

linglong::util::Error OSTreeRepo::checkout(const package::Ref &ref, const QString &subPath, const QString &target)
{
    QStringList args = {"checkout", "--union"};
    if (!subPath.isEmpty()) {
        args.push_back("--subpath=" + subPath);
    }
    args.append({ref.toString(), target});
    return WrapError(dd_ptr->ostreeRun(args));
}

linglong::util::Error OSTreeRepo::checkoutAll(const package::Ref &ref, const QString &subPath, const QString &target)
{
    Q_D(OSTreeRepo);

    QStringList runtimeArgs = {"checkout", "--union", QStringList {ref.toString(), "runtime"}.join("/"), target};
    QStringList develArgs = {"checkout", "--union", QStringList {ref.toString(), "devel"}.join("/"), target};

    if (!subPath.isEmpty()) {
        runtimeArgs.push_back("--subpath=" + subPath);
        develArgs.push_back("--subpath=" + subPath);
    }

    auto ret = d->ostreeRun(runtimeArgs);
    if (!ret.success()) {
        return NewError(ret);
    }

    ret = d->ostreeRun(develArgs);

    return WrapError(ret);
}

QString OSTreeRepo::rootOfLayer(const package::Ref &ref)
{
    return QStringList {dd_ptr->path, "layers", ref.appId, ref.version, ref.arch}.join(QDir::separator());
}

bool OSTreeRepo::isRefExists(const package::Ref &ref)
{
    Q_D(OSTreeRepo);
    auto runtimeRef = ref.toString() + '/' + "runtime";
    auto ret = runner::Runner("sh", {"-c", QString("ostree refs --repo=%1 | grep -Fx %2").arg(d->ostreePath).arg(runtimeRef)}, -1);

    return ret;
}

package::Ref OSTreeRepo::latestOfRef(const QString &appId, const QString &appVersion)
{
    auto latestVersionOf = [this](const QString &appId) {
        auto localRepoRoot = QString(dd_ptr->path) + "/layers" + "/" + appId;

        QDir appRoot(localRepoRoot);

        // found latest
        if (appRoot.exists("latest")) {
            return appRoot.absoluteFilePath("latest");
        }

        // FIXME: found biggest version
        appRoot.setSorting(QDir::Name | QDir::Reversed);
        auto verDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        auto available = verDirs.value(0);
        for (auto item : verDirs) {
            linglong::util::AppVersion versionIter(item);
            linglong::util::AppVersion dstVersion(available);
            if (versionIter.isValid() && versionIter.isBigThan(dstVersion)) {
                available = item;
            }
        }
        qDebug() << "available version" << available << appRoot << verDirs;
        return available;
    };

    // 未指定版本使用最新版本，指定版本下使用指定版本
    QString version;
    if (!appVersion.isEmpty()) {
        version = appVersion;
    } else {
        version = latestVersionOf(appId);
    }
    auto ref = appId + "/" + version + "/" + util::hostArch();
    return package::Ref(ref);
}

linglong::util::Error OSTreeRepo::removeRef(const package::Ref &ref)
{
    QStringList args = {"refs", "--delete", ref.toString()};
    auto ret = dd_ptr->ostreeRun(args);
    if (!ret.success()) {
        return WrapError(ret, "delete refs failed");
    }

    args = QStringList {"prune"};
    return WrapError(dd_ptr->ostreeRun(args));
}

std::tuple<linglong::util::Error, QStringList> OSTreeRepo::remoteList()
{
    QStringList remoteList;
    QStringList args = {"remote", "list"};
    QByteArray output;
    auto ret = dd_ptr->ostreeRun(args, &output);
    if (!ret.success()) {
        return {WrapError(ret, "remote list failed"), QStringList {}};
    }

    for (const auto &item : QString::fromLocal8Bit(output).trimmed().split('\n')) {
        if (!item.trimmed().isEmpty()) {
            remoteList.push_back(item);
        }
    }

    return {NoError(), remoteList};
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace repo
} // namespace linglong
