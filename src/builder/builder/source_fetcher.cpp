/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "source_fetcher.h"

#include "builder_config.h"
#include "module/util/error.h"
#include "module/util/file.h"
#include "module/util/http/http_client.h"
#include "module/util/runner.h"
#include "project.h"
#include "source_fetcher_p.h"

#include <QNetworkRequest>

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

    QMap<QString, std::function<linglong::util::Error(const QString &path, const QString &dir)>>
            subcommandMap = {
                { CompressedFileTarXz,
                  [](const QString &path, const QString &dir) -> linglong::util::Error {
                      auto ret = runner::Runner("tar", { "-C", dir, "-xvf", path }, -1);
                      if (!ret) {
                          return NewError(-1, QString("extract %1 failed").arg(path));
                      }
                      return Success();
                  } },
            };

    auto suffix = fixSuffix(fi);

    if (subcommandMap.contains(suffix)) {
        auto subcommand = subcommandMap[suffix];
        return subcommand(path, dir);
    } else {
        qCritical() << "unsupported suffix" << path << fi.completeSuffix() << fi.suffix()
                    << fi.bundleName();
    }
    return NewError(-1, "unkonwn error");
}

SourceFetcherPrivate::SourceFetcherPrivate(Source *src, SourceFetcher *parent)
    : project(nullptr)
    , source(src)
    , file(nullptr)
    , q_ptr(parent)
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
            QStringList{
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

    q->setSourceRoot(sourceTargetPath());

    QUrl url(source->url);
    auto path = BuilderConfig::instance()->targetFetchCachePath() + "/" + filename();

    file.reset(new QFile(path));
    if (!file->open(QIODevice::WriteOnly)) {
        return NewError(-1, file->errorString());
    }

    QNetworkRequest request;
    request.setUrl(url);

    auto reply = util::networkMgr().get(request);

    QObject::connect(reply, &QNetworkReply::metaDataChanged, [reply]() {
        qDebug() << reply->header(QNetworkRequest::ContentLengthHeader);
    });

    QObject::connect(reply, &QNetworkReply::readyRead, [this, reply]() {
        file->write(reply->readAll());
    });

    QEventLoop loop;
    QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    // 将缓存写入文件
    file->close();

    if (source->digest != util::fileHash(path, QCryptographicHash::Sha256)) {
        qCritical() << QString("mismatched hash: %1")
                               .arg(util::fileHash(path, QCryptographicHash::Sha256));
        return NewError(-1, "download failed");
    }

    return q->extractFile(path, sourceTargetPath());
}

// TODO: DO NOT clone all repo, see here for more:
// https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset/3489576
util::Error SourceFetcherPrivate::fetchGitRepo()
{
    Q_Q(SourceFetcher);

    q->setSourceRoot(sourceTargetPath());
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

    return Success();
};

util::Error SourceFetcherPrivate::handleLocalPatch()
{
    // apply local patch
    qInfo() << QString("finding local patch");
    if (source->patch.isEmpty()) {
        qInfo() << QString("nothing to patch");
        return Success();
    }

    for (auto localPatch : source->patch) {
        if (localPatch.isEmpty()) {
            qWarning() << QString("this patch is empty, check it");
            continue;
        }
        qInfo() << QString("applying patch: %1").arg(localPatch);
        if (!runner::Runner("patch",
                            { "-p1", "-i", project->config().absoluteFilePath({ localPatch }) },
                            -1)) {
            return NewError(-1, "patch failed");
        }
    }

    return Success();
}

util::Error SourceFetcherPrivate::handleLocalSource()
{
    Q_Q(SourceFetcher);

    q->setSourceRoot(BuilderConfig::instance()->getProjectRoot());
    return Success();
}

util::Error SourceFetcher::patch()
{
    Q_D(SourceFetcher);

    util::Error ret;

    QDir::setCurrent(sourceRoot());

    // TODO: remove later
    // if (QDir("debian").exists()) {
    //    WrapError(d->handleDebianPatch());
    //}

    return d->handleLocalPatch();
}

linglong::util::Error SourceFetcher::fetch()
{
    Q_D(SourceFetcher);

    QSharedPointer<util::Error> ret;

    if (d->source->kind == "git") {
        ret.reset(new util::Error(d->fetchGitRepo()));
    } else if (d->source->kind == "archive") {
        ret.reset(new util::Error(d->fetchArchiveFile()));
    } else if (d->source->kind == "local") {
        ret.reset(new util::Error(d->handleLocalSource()));
    } else {
        return NewError(-1, "unknown source kind");
    }

    if (!ret->success()) {
        return *ret;
    }

    return patch();
}

QString SourceFetcher::sourceRoot() const
{
    return srcRoot;
}

void SourceFetcher::setSourceRoot(const QString &path)
{
    srcRoot = path;
}

SourceFetcher::SourceFetcher(Source *s, Project *project)
    : dd_ptr(new SourceFetcherPrivate(s, this))
    , CompressedFileTarXz("tar.xz")
{
    Q_D(SourceFetcher);

    d->project = project;
}

SourceFetcher::~SourceFetcher() { }
} // namespace builder
} // namespace linglong
