/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_P_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_P_H_

#include "ostree_repo.h"

#include "module/package/package.h"
#include "module/repo/repo.h"
#include "module/repo/ostree_repohelper.h"
#include "module/util/http/http_client.h"
#include "module/util/http/httpclient.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-async-progress.h>
#include <ostree-repo.h>

#include <QPointer>
#include <QScopedPointer>

namespace linglong {
namespace repo {

struct OstreeRepoObject
{
    QString objectName;
    QString rev;
    QString path;
};

typedef QMap<QString, OstreeRepoObject> RepoObjectMap;

class OSTreeRepo;
class InfoResponse;
class UploadRequest;
class UploadTaskRequest;
class OSTreeRepoPrivate
{
public:
    ~OSTreeRepoPrivate() = default;

private:
    OSTreeRepoPrivate(QString localRepoRootPath,
                      QString remoteEndpoint,
                      QString remoteRepoName,
                      OSTreeRepo *parent);

    static OstreeRepo *openRepo(const QString &path);

    static QByteArray glibBytesToQByteArray(GBytes *bytes);

    static GBytes *glibInputStreamToBytes(GInputStream *inputStream);

    static std::tuple<util::Error, QByteArray> compressFile(const QString &filepath);

    linglong::util::Error ostreeRun(const QStringList &args, QByteArray *stdout);

    QString getObjectPath(const QString &objName);

    // FIXME: return {Error, QStringList}
    QStringList traverseCommit(const QString &rev, int maxDepth);

    std::tuple<QList<OstreeRepoObject>, util::Error> findObjectsOfCommits(const QStringList &revs);

    std::tuple<QString, util::Error> resolveRev(const QString &ref);

    util::Error pull(const QString &ref);

    InfoResponse *getRepoInfo(const QString &repoName);

    std::tuple<QString, util::Error> newUploadTask(UploadRequest *req);

    std::tuple<QString, util::Error> newUploadTask(const QString &repoName, UploadTaskRequest *req);

    util::Error doUploadTask(const QString &taskID, const QString &filePath);

    util::Error doUploadTask(const QString &repoName,
                             const QString &taskID,
                             const QList<OstreeRepoObject> &objects);

    util::Error cleanUploadTask(const QString &repoName, const QString &taskID);

    util::Error cleanUploadTask(const package::Ref &ref, const QString &filePath);

    util::Error getUploadStatus(const QString &taskID);

    util::Error getToken();

private:
    QString repoRootPath;
    QString remoteEndpoint;
    QString remoteRepoName;

    QString remoteToken;

    OstreeRepo *repoPtr = nullptr;
    QString ostreePath;

    util::HttpRestClient httpClient;

    OSTreeRepo *q_ptr;
    Q_DECLARE_PUBLIC(OSTreeRepo);
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_P_H_
