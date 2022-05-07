/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "source_fetcher.h"
#include "source_fetcher_p.h"

namespace linglong {
namespace builder {

QString SourceFetcher::fixSuffix(const QFileInfo &fi)
{
    if (fi.completeSuffix().endsWith(CompressedFileTarXz)) {
        return CompressedFileTarXz;
    }
    return fi.suffix();
}

linglong::util::Error SourceFetcher::extractFile(const QString &path, const QString &dir)
{
    QFileInfo fi(path);

    QMap<QString, std::function<linglong::util::Error(const QString &path, const QString &dir)>> subcommandMap = {
        {CompressedFileTarXz,
         [](const QString &path, const QString &dir) -> linglong::util::Error {
             auto ret = runner::Runner("tar", {"-C", dir, "-xvf", path}, -1);
             if (!ret) {
                 return NewError(-1, "extract " + path + "failed");
             }
             return NoError();
         }},
    };

    auto suffix = fixSuffix(fi);

    if (subcommandMap.contains(suffix)) {
        auto subcommand = subcommandMap[suffix];
        return subcommand(path, dir);
    } else {
        qCritical() << "unsupported suffix" << path << fi.completeSuffix() << fi.suffix() << fi.bundleName();
    }
    return NewError(-1, "unkonwn error");
}

SourceFetcherPrivate::SourceFetcherPrivate(Source *src, SourceFetcher *parent)
    : project(nullptr), source(src), file(nullptr), q_ptr(parent)
{
}
  
QString SourceFetcherPrivate::filename()
{
    QUrl url(source->url);
    return url.fileName();
}

// TODO: use share cache for all project
QString SourceFetcherPrivate::sourceTargetPath() const
{
    auto path =
        QStringList {
            BuilderConfig::instance()->targetSourcePath(),
            //                source->commit,
        }
            .join("/");
    util::ensureDir(path);
    return path;
}

linglong::util::Error SourceFetcherPrivate::fetchArchiveFile()
{
    Q_Q(SourceFetcher);

    QUrl url(source->url);
    auto path = BuilderConfig::instance()->targetFetchCachePath() + "/" + filename();

    file.reset(new QFile(path));
    if (!file->open(QIODevice::WriteOnly)) {
        return NewError(-1, file->errorString());
    }

    QNetworkRequest request;
    request.setUrl(url);

    auto reply = util::networkMgr().get(request);

    QObject::connect(reply, &QNetworkReply::metaDataChanged,
                     [reply]() { qDebug() << reply->header(QNetworkRequest::ContentLengthHeader); });

    QObject::connect(reply, &QNetworkReply::readyRead, [this, reply]() { file->write(reply->readAll()); });

    QEventLoop loop;
    QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    return q->extractFile(path, sourceTargetPath());
}

// TODO: DO NOT clone all repo, see here for more:
// https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset/3489576
linglong::util::Error SourceFetcherPrivate::fetchGitRepo() const
{
    util::ensureDir(sourceTargetPath());

    if (!QDir::setCurrent(sourceTargetPath())) {
        return NewError(-1, QString("change to %1 failed").arg(sourceTargetPath()));
    }

    if (!runner::Runner("git",
                        {
                            "clone",
                            source->url,
                            sourceTargetPath(),
                        },
                        -1)) {
        qDebug() << NewError(-1, "git clone failed");
    }

    QDir::setCurrent(sourceTargetPath());

    if (!runner::Runner("git",
                        {
                            "checkout",
                            "-b",
                            source->version,
                            source->commit,
                        },
                        -1)) {
        qDebug() << NewError(-1, "git checkout failed");
    }

    if (!runner::Runner("git",
                        {
                            "reset",
                            "--hard",
                            source->commit,
                        },
                        -1)) {
        return NewError(-1, "git reset failed");
    }

    return NoError();
};

linglong::util::Error SourceFetcherPrivate::handleDebianPatch() const
{
    qInfo() << "debian source here, find debian patch ...";

    QString patchPath = "debian/patches";
    QString seriesPath = QStringList {patchPath, "series"}.join("/");

    auto result = runner::RunnerRet("sh", {"-c", QString("cat %1 | grep -e '^[0-9:a-z:A-Z]'").arg(seriesPath)}, -1);

    if (std::get<0>(result)) {
        for (auto patchName : std::get<1>(result)) {
            if (patchName.isEmpty()) {
                // the data from stdout maybe null
                continue;
            }

            auto debianPatch = QStringList {sourceTargetPath(), patchPath, patchName}.join("/");

            qDebug() << QString("applying debian patch: %1").arg(patchName);
            if (!runner::Runner("patch", {"-p1", "-i", debianPatch}, -1)) {
                return NewError(-1, "patch failed");
            }
        }
    }

    return NoError();
}

linglong::util::Error SourceFetcherPrivate::handleLocalPatch() const
{
    // apply local patch
    qInfo() << "find local patch ...";
    if (source->patch.size() == 0) {
        qInfo() << "Nothing to patch.";
        return NoError();
    }

    for (auto localPatch : source->patch) {
        if (localPatch.isEmpty()) {
            qWarning() << "This patch is empty, check it.";
            continue;
        }
        qDebug() << QString("Applying patch: %1").arg(localPatch);
        if (!runner::Runner("patch", {"-p1", "-i", project->config().absoluteFilePath({localPatch})}, -1)) {
            return NewError(-1, "patch failed");
        }
    }

    return NoError();
}

linglong::util::Error SourceFetcher::patch()
{
    linglong::util::Error ret(NoError());

    QDir::setCurrent(dd_ptr->sourceTargetPath());

    // TODO: remove later
    if (QDir("debian").exists()) {
        WrapError(dd_ptr->handleDebianPatch());
    }

    return dd_ptr->handleLocalPatch();
}

linglong::util::Error SourceFetcher::fetch()
{
    linglong::util::Error ret(NoError());

    if (dd_ptr->source->kind == "git") {
        ret = dd_ptr->fetchGitRepo();
    } else if (dd_ptr->source->kind == "archive") {
        ret = dd_ptr->fetchArchiveFile();
    } else {
        return NewError(-1, "unknown source kind");
    }

    if (!ret.success()) {
        return ret;
    }

    return patch();
}

QString SourceFetcher::sourceRoot()
{
    return dd_ptr->sourceTargetPath();
}

SourceFetcher::SourceFetcher(Source *s, Project *project)
    : dd_ptr(new SourceFetcherPrivate(s, this))
    ,CompressedFileTarXz("tar.xz")
{
    dd_ptr->project = project;
}

SourceFetcher::~SourceFetcher()
{
}
} // namespace builder
} // namespace linglong
