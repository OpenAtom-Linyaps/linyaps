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

#include <QEventLoop>
#include <QFileInfo>
#include <QUrl>

#include "module/util/http_client.h"
#include "module/util/runner.h"
#include "module/util/fs.h"
#include "module/util/result.h"

#include "project.h"
#include "builder_config.h"

namespace linglong {
namespace builder {

const char *CompressedFileTarXz = "tar.xz";

QString fixSuffix(const QFileInfo &fi)
{
    if (fi.completeSuffix().endsWith(CompressedFileTarXz)) {
        return CompressedFileTarXz;
    }
    return fi.suffix();
}

util::Error extractFile(const QString &path, const QString &dir)
{
    QFileInfo fi(path);

    QMap<QString, std::function<util::Error(const QString &path, const QString &dir)>> subcommandMap = {
        {CompressedFileTarXz,
         [](const QString &path, const QString &dir) -> util::Error {
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

class SourceFetcherPrivate
{
public:
    explicit SourceFetcherPrivate(Source *s)
        : source(s)
    {
    }

    QString filename()
    {
        QUrl url(source->url);
        return url.fileName();
    }

    // TODO: use share cache for all project
    QString sourceTargetPath() const
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

    util::Error fetchArchiveFile()
    {
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
                         [this, reply]() { qDebug() << reply->header(QNetworkRequest::ContentLengthHeader); });

        QObject::connect(reply, &QNetworkReply::readyRead, [this, reply]() { file->write(reply->readAll()); });

        QEventLoop loop;
        QEventLoop::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        return extractFile(path, sourceTargetPath());
    }

    // TODO: DO NOT clone all repo, see here for more:
    // https://stackoverflow.com/questions/3489173/how-to-clone-git-repository-with-specific-revision-changeset/3489576
    util::Error fetchGitRepo() const
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

        // apply patch
        if (source->patch.size() == 0) {
            qInfo() << "Nothing to patch.";
            return NoError();
        }
      
        for(auto localPatch : source->patch) {
            if (localPatch.isEmpty()) {
                qWarning() << "This patch is empty, check it.";
                continue;
            }
            qDebug() << QString("Applying patch: %1").arg(localPatch);
            if (!runner::Runner("patch",
                                {
                                    "-p1",
                                    "-i",
                                    project->config().absoluteFilePath({localPatch})
                                },
                                -1)) {
                return NewError(-1, "patch failed");
            }
        }

        return NoError();
    };

    Project *project;
    Source *source;
    QScopedPointer<QFile> file;
};

util::Error SourceFetcher::fetch()
{
    if (dd_ptr->source->kind == "git") {
        return dd_ptr->fetchGitRepo();
    } else if (dd_ptr->source->kind == "archive") {
        return dd_ptr->fetchArchiveFile();
    }

    return NewError(-1, "unknown source kind");
}

QString SourceFetcher::sourceRoot()
{
    return dd_ptr->sourceTargetPath();
}

SourceFetcher::SourceFetcher(Source *s, Project *project)
    : dd_ptr(new SourceFetcherPrivate(s))
{
    dd_ptr->project = project;
}

SourceFetcher::~SourceFetcher() = default;

} // namespace builder
} // namespace linglong
