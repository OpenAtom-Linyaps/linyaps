/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "source_fetcher.h"

#include "builder_config.h"
#include "linglong/cli/printer.h"
#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/http/http_client.h"
#include "linglong/util/runner.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"
#include "project.h"
#include "source_fetcher_p.h"

#include <QNetworkRequest>
#include <QTemporaryDir>

namespace linglong {
namespace builder {

QString SourceFetcherPrivate::fixSuffix(const QFileInfo &fi)
{
    Q_Q(SourceFetcher);
    for (const char *suffix : { q->CompressedFileTarXz,
                                q->CompressedFileTarBz2,
                                q->CompressedFileTarGz,
                                q->CompressedFileTgz,
                                q->CompressedFileTar }) {
        if (fi.completeSuffix().endsWith(suffix)) {
            return q->CompressedFileTar;
        }
    }
    return fi.suffix();
}

linglong::util::Error SourceFetcherPrivate::extractFile(const QString &path, const QString &dir)
{
    Q_Q(SourceFetcher);
    QFileInfo fi(path);

    auto tarxz = [](const QString &path, const QString &dir) -> linglong::util::Error {
        return WrapError(util::Exec("tar", { "-C", dir, "-xvf", path }),
                         QString("extract %1 failed").arg(path));
    };
    auto targz = [](const QString &path, const QString &dir) -> linglong::util::Error {
        return WrapError(util::Exec("tar", { "-C", dir, "-zxvf", path }),
                         QString("extract %1 failed").arg(path));
    };
    auto tarbz2 = [](const QString &path, const QString &dir) -> linglong::util::Error {
        return WrapError(util::Exec("tar", { "-C", dir, "-jxvf", path }),
                         QString("extract %1 failed").arg(path));
    };
    auto zip = [](const QString &path, const QString &dir) -> linglong::util::Error {
        return WrapError(util::Exec("unzip", { "-d", dir, path }),
                         QString("extract %1 failed").arg(path));
    };

    QMap<QString, std::function<linglong::util::Error(const QString &path, const QString &dir)>>
      subcommandMap = {
          { q->CompressedFileTarXz, tarxz },   { q->CompressedFileTarGz, targz },
          { q->CompressedFileTarBz2, tarbz2 }, { q->CompressedFileZip, zip },
          { q->CompressedFileTgz, targz },     { q->CompressedFileTar, tarxz },
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
} // namespace builder

SourceFetcherPrivate::SourceFetcherPrivate(QSharedPointer<Source> src, SourceFetcher *parent)
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

std::tuple<QString, linglong::util::Error> SourceFetcherPrivate::fetchArchiveFile()
{
    Q_Q(SourceFetcher);

    QUrl url(source->url);
    auto path = BuilderConfig::instance()->targetFetchCachePath() + "/" + filename();

    file.reset(new QFile(path));
    // if file exists, check digest
    if (file->exists()) {
        if (source->digest == util::fileHash(path, QCryptographicHash::Sha256)) {
            q->printer.printMessage(QString("file %1 exists, skip download").arg(path));
            return { path, Success() };
        }

        q->printer.printMessage(
          QString("file %1 exists, but hash mismatched, redownload").arg(path));
        file->remove();
    }

    if (!file->open(QIODevice::WriteOnly)) {
        return { "", NewError(-1, file->errorString()) };
    }

    QNetworkRequest request;
    request.setUrl(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Wget/1.21.4");

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
        qCritical()
          << QString("mismatched hash: %1").arg(util::fileHash(path, QCryptographicHash::Sha256));
        return { "", NewError(-1, "download failed") };
    }

    return { path, Success() };
}

utils::error::Result<void> SourceFetcherPrivate::fetchGitRepo()
{
    Q_Q(SourceFetcher);
    LINGLONG_TRACE("fetchGitRepo");
    q->printer.printMessage(QString("Path: %1").arg(q->sourceRoot()), 2);

    // 如果二进制安装在系统目录中，优先使用系统中安装的脚本文件（便于用户更改），否则使用二进制内嵌的脚本（便于开发调试）
    auto scriptFile = QString(LINGLONG_LIBEXEC_DIR) + "/fetch-git-repo.sh";
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        qWarning() << "Dumping fetch-git-repo from qrc...";
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath("fetch-git-repo");
        QFile::copy(":/scripts/fetch-git-repo", scriptFile);
    }
    QStringList args = {
        scriptFile, q->sourceRoot(), source->url, source->commit, source->version,
    };
    q->printer.printMessage(QString("Command: sh %1").arg(args.join(" ")), 2);

    QSharedPointer<QByteArray> output = QSharedPointer<QByteArray>::create();
    auto err = util::Exec("sh", args, -1, output);
    if (err) {
        q->printer.printMessage("download source failed." + err.message(), 2);
        return LINGLONG_ERR("download source failed." + err.message());
    }
    if (!dir.isNull()) {
        dir->remove();
    }
    return LINGLONG_OK;
}

utils::error::Result<void> SourceFetcherPrivate::fetchDscRepo()
{
    Q_Q(SourceFetcher);
    LINGLONG_TRACE("fetch dsc repo");
    q->printer.printMessage(QString("Path: %1").arg(q->sourceRoot()), 2);

    // 如果二进制安装在系统目录中，优先使用系统中安装的脚本文件（便于用户更改），否则使用二进制内嵌的脚本（便于开发调试）
    auto scriptFile = QString(LINGLONG_LIBEXEC_DIR) + "/fetch-dsc-repo";
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        qWarning() << "Dumping fetch-dsc-repo from qrc...";
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath("fetch-dsc-repo");
        QFile::copy(":/scripts/fetch-dsc-repo", scriptFile);
    }
    QStringList args = {
        scriptFile,
        q->sourceRoot(),
        source->url,
    };
    q->printer.printMessage(QString("Command: sh %1").arg(args.join(" ")), 2);

    QSharedPointer<QByteArray> output = QSharedPointer<QByteArray>::create();
    auto err = util::Exec("sh", args, -1, output);
    if (err) {
        q->printer.printMessage("download source failed." + err.message(), 2);
        return LINGLONG_ERR(err.message(), err.code());
    }
    if (!dir.isNull()) {
        dir->remove();
    }
    return LINGLONG_OK;
}

util::Error SourceFetcherPrivate::handleLocalPatch()
{
    Q_Q(SourceFetcher);
    // apply local patch
    q->printer.printReplacedText("Finding local patch ...", 2);
    if (source->patch.isEmpty()) {
        q->printer.printReplacedText("Nothing to patch\n", 2);
        return Success();
    }

    for (auto localPatch : source->patch) {
        if (localPatch.isEmpty()) {
            qWarning() << QString("this patch is empty, check it");
            continue;
        }
        q->printer.printReplacedText(QString("Applying %1 ...").arg(localPatch), 2);
        if (auto err =
              util::Exec("patch",
                         { "-p1", "-i", project->config().absoluteFilePath({ localPatch }) })) {
            return NewError(err, "patch failed");
        }
        q->printer.printReplacedText(QString("Applying %1 done\n").arg(localPatch), 2);
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

utils::error::Result<void> SourceFetcher::fetch()
{
    LINGLONG_TRACE("source fetch");
    Q_D(SourceFetcher);
    if (d->source->kind == "git") {
        printer.printMessage(QString("Source: %1").arg(d->source->url), 2);
        auto ret = d->fetchGitRepo();
        if (!ret.has_value()) {
            return LINGLONG_ERR(ret);
        }
    } else if (d->source->kind == "dsc") {
        printer.printMessage(QString("Source: %1").arg(d->source->url), 2);
        auto ret = d->fetchDscRepo();
        if (!ret) {
            return ret;
        }
    } else if (d->source->kind == "archive") {
        printer.printMessage(QString("Source: %1").arg(d->source->url), 2);
        QString s;
        util::Error err;
        std::tie(s, err) = d->fetchArchiveFile();
        if (err) {
            return LINGLONG_ERR(err.message());
        }
        err = d->extractFile(s, sourceRoot());
    } else if (d->source->kind == "local") {
        // deprecated
    } else if (d->source->kind == "file") {
        printer.printMessage(QString("Source: %1").arg(d->source->url), 2);
        QString s;
        util::Error err;
        std::tie(s, err) = d->fetchArchiveFile();
        if (err) {
            return LINGLONG_ERR(err.message());
        }

        QFile::copy(BuilderConfig::instance()->targetFetchCachePath() + "/" + d->filename(),
                    BuilderConfig::instance()->targetSourcePath() + "/" + d->filename());
    } else {
        return LINGLONG_ERR("unknown source kind");
    }
    auto err = patch();
    if (err) {
        return LINGLONG_ERR(err.message());
    }
    return LINGLONG_OK;
}

QString SourceFetcher::sourceRoot() const
{
    return srcRoot;
}

void SourceFetcher::setSourceRoot(const QString &path)
{
    srcRoot = path;
}

SourceFetcher::SourceFetcher(QSharedPointer<Source> s, cli::Printer &p, Project *project)
    : dd_ptr(new SourceFetcherPrivate(s, this))
    , printer(p)
{
    Q_D(SourceFetcher);

    d->project = project;
    if (d->source->kind == "git" && d->source->version.isEmpty()) {
        d->source->version = project->package->version;
    }
    setSourceRoot(d->sourceTargetPath());
}

SourceFetcher::~SourceFetcher() { }
} // namespace builder
} // namespace linglong
