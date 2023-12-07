/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "linglong/package/info.h"
#include "linglong/package/ref.h"
#include "linglong/repo/repo.h"
#include "linglong/util/error.h"
#include "linglong/util/runner.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/version/semver.h"
#include "linglong/util/version/version.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>
#include <qdir.h>

#include <QDir>
#include <QProcess>
#include <QtWebSockets/QWebSocket>

#include <cstddef>
#include <utility>

#include <fcntl.h>

namespace linglong {
namespace repo {

namespace {
const int MAX_ERRINFO_BUFSIZE = 512;
}

QSERIALIZER_IMPL(InfoResponse);
QSERIALIZER_IMPL(RevPair);
QSERIALIZER_IMPL(UploadResponseData);

QSERIALIZER_IMPL(UploadTaskRequest);
QSERIALIZER_IMPL(UploadRequest);
QSERIALIZER_IMPL(UploadTaskResponse);

typedef QMap<QString, OstreeRepoObject> RepoObjectMap;

OSTreeRepo::OSTreeRepo(const QString &localRepoPath,
                       const QString &remoteEndpoint,
                       const QString &remoteRepoName,
                       api::client::ClientApi &client)
    : repoRootPath(QDir(localRepoPath).absolutePath())
    , remoteEndpoint(remoteEndpoint)
    , remoteRepoName(remoteRepoName)
    , repoClient(client)
{
    ostreePath = repoRootPath + "/repo";
    qDebug() << "ostree repo path is" << ostreePath;

    // FIXME(black_desk): Just a quick hack to make sure openRepo called after the repo is
    // created. This function might failed as the caller might not have right permission to
    // create that directory or modify files in this directory. This should be fixed by split
    // Repo into read-only Repo and modifiable Repo, and void using modifiable Repo in
    // ll-service.
    auto ret = initCreateRepoIfNotExists();
    if (ret.has_value()) {
        return;
    }

    qDebug().noquote() << "Failed to create ostree based linglong repo." << Qt::endl
                       << ret.error().message();

    g_autoptr(GError) gErr = nullptr;

    g_autoptr(GFile) repoPath = g_file_new_for_path(ostreePath.toLocal8Bit());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(repoPath);
    if (!ostree_repo_open(repo, nullptr, &gErr)) {
        qCritical().noquote() << "open repo" << ostreePath << "failed"
                              << QString::fromStdString(std::string(gErr->message));
    }
    Q_ASSERT(repo);
    repoPtr.reset(g_steal_pointer(&repo));
}

linglong::utils::error::Result<void> OSTreeRepo::importDirectory(const package::Ref &ref,
                                                                 const QString &path)
{
    g_autoptr(GError) gErr = nullptr;
    if (!ostree_repo_prepare_transaction(repoPtr.get(), NULL, NULL, &gErr))
        return LINGLONG_ERR(gErr->code, gErr->message);

    g_autoptr(OstreeMutableTree) mtree = ostree_mutable_tree_new();

    g_autoptr(OstreeRepoCommitModifier) modifier = nullptr;
    modifier =
      ostree_repo_commit_modifier_new(OSTREE_REPO_COMMIT_MODIFIER_FLAGS_CANONICAL_PERMISSIONS,
                                      nullptr,
                                      nullptr,
                                      nullptr);

    g_autoptr(GFile) gfile = g_file_new_for_path(path.toStdString().c_str());
    if (!ostree_repo_write_directory_to_mtree(repoPtr.get(),
                                              gfile,
                                              mtree,
                                              modifier,
                                              nullptr,
                                              &gErr))
        return LINGLONG_ERR(gErr->code, gErr->message);

    g_autoptr(OstreeRepoFile) repo_file = nullptr;
    if (!ostree_repo_write_mtree(repoPtr.get(), mtree, (GFile **)&repo_file, nullptr, &gErr))
        return LINGLONG_ERR(gErr->code, gErr->message);

    g_autofree char *out_commit = nullptr;
    if (!ostree_repo_write_commit(repoPtr.get(),
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  repo_file,
                                  &out_commit,
                                  NULL,
                                  &gErr)) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    ostree_repo_transaction_set_ref(repoPtr.get(),
                                    NULL,
                                    ref.toOSTreeRefLocalString().toStdString().c_str(),
                                    out_commit);
    ostree_repo_commit_transaction(repoPtr.get(), NULL, NULL, &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    return LINGLONG_OK;
}

linglong::utils::error::Result<void> OSTreeRepo::push(const package::Ref &ref)
{
    auto ret = getToken();
    if (!ret.has_value()) {
        return LINGLONG_ERR(-1, "get token failed");
    }
    QSharedPointer<UploadRequest> uploadReq(new UploadRequest);

    uploadReq->repoName = remoteRepoName;
    uploadReq->ref = ref.toOSTreeRefLocalString();

    // FIXME: no need,use /v1/meta/:id
    QSharedPointer<InfoResponse> repoInfo(getRepoInfo(remoteRepoName));
    if (repoInfo->code != 200) {
        return LINGLONG_ERR(-1, repoInfo->msg);
    }

    QString taskID;
    {
        auto ret = newUploadTask(uploadReq);
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("call newUploadTask failed", ret.error());
        }
        taskID = *ret;
    }

    // compress form data
    QString filePath;
    {
        auto ret = compressOstreeData(ref);
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("compress ostree data failed", ret.error());
        }
        if (ret.has_value()) {
            filePath = *ret;
        }
    }

    {
        auto ret = doUploadTask(taskID, filePath);
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("call doUploadTask failed", ret.error());
        }
    }

    {
        auto ret = getUploadStatus(taskID);
        if (!ret.has_value()) {
            cleanUploadTask(ref, filePath);
            return ret;
        }
    }

    return LINGLONG_EWRAP("call cleanUploadTask failed", cleanUploadTask(ref, filePath).error());
}

linglong::utils::error::Result<void> OSTreeRepo::push(const package::Ref &ref, bool /*force*/)
{
    {
        auto ret = getToken();
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("get token failed", ret.error());
        }
    }

    QSharedPointer<UploadTaskRequest> uploadTaskReq(new UploadTaskRequest);

    // FIXME: no need,use /v1/meta/:id
    QSharedPointer<InfoResponse> repoInfo(getRepoInfo(remoteRepoName));
    if (repoInfo->code != 200) {
        return LINGLONG_ERR(-1, repoInfo->msg);
    }

    QString commitID;
    {
        auto ret = resolveRev(ref.toOSTreeRefLocalString());
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("push failed:" + ref.toOSTreeRefLocalString(), ret.error());
        }
        if (ret.has_value()) {
            commitID = *ret;
        }
    }
    qDebug() << "push commit" << commitID << ref.toOSTreeRefLocalString();

    auto revPair = QSharedPointer<RevPair>(new RevPair);

    // upload msg, should specific channel in ref
    uploadTaskReq->refs[ref.toOSTreeRefLocalString()] = revPair;
    revPair->client = commitID;
    // FIXME: get server version to compare
    revPair->server = "";

    QList<OstreeRepoObject> objects;
    {
        // find files to commit
        auto ret = findObjectsOfCommits({ commitID });
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("call findObjectsOfCommits failed", ret.error());
        }

        objects = *ret;
    }

    for (auto const &obj : objects) {
        uploadTaskReq->objects.push_back(obj.objectName);
    }

    // send files
    QString taskID;
    {
        auto ret = newUploadTask(remoteRepoName, uploadTaskReq);
        if (!ret.has_value()) {
            return LINGLONG_EWRAP("call newUploadTask failed", ret.error());
        }
        taskID = *ret;
    }

    {
        auto ret = doUploadTask(remoteRepoName, taskID, objects);
        if (!ret.has_value()) {
            cleanUploadTask(remoteRepoName, taskID);
            return LINGLONG_EWRAP("call newUploadTask failed", ret.error());
        }
    }

    return LINGLONG_EWRAP("call cleanUploadTask failed",
                          cleanUploadTask(remoteRepoName, taskID).error());
}

linglong::utils::error::Result<void> OSTreeRepo::pull(package::Ref &ref, bool /*force*/)
{
    // FIXME(black_desk): should implement force

    // FIXME(black_desk): When a error raised from libcurl, libostree will treat
    // it like a fail, but not a temporary error, which make the default retry
    // (5 times) useless. So we now have to retry some times to overcome this
    // problem.
    // As we have try the current base will fail so many times during
    // transferring. So we decide to retry 30 times.
    int retry = 30;
    g_autoptr(GError) gErr = nullptr;
    auto str = ref.toLocalRefString().toLocal8Bit();
    char *refs_to_fetch[2] = { str.data(), nullptr };
    while (retry--) {
        qDebug() << "remaining retries" << retry;
        g_clear_error(&gErr);
        ostree_repo_pull(repoPtr.get(),
                         remoteRepoName.toLocal8Bit(),
                         refs_to_fetch,
                         OSTREE_REPO_PULL_FLAGS_NONE,
                         NULL,
                         NULL,
                         &gErr);
        if (!gErr) {
            return LINGLONG_OK;
        }
        qWarning() << gErr->code << gErr->message;
    }
    return LINGLONG_ERR(
      -1,
      QString("pull branch %1 from repo %2 failed!").arg(ref.toLocalString(), remoteRepoName));
}

linglong::utils::error::Result<void> OSTreeRepo::listRemoteRefs()
{
    g_autoptr(GHashTable) refs = NULL;
    g_autoptr(GError) gErr = NULL;
    ostree_repo_remote_list_refs(repoPtr.get(),
                                 remoteRepoName.toLocal8Bit(),
                                 &refs,
                                 NULL,
                                 &gErr);
    if (gErr) {
        qWarning() << gErr->code << gErr->message;
        return LINGLONG_OK;
    }
    auto ordered_keys = g_hash_table_get_keys(refs);
    ordered_keys = g_list_sort(ordered_keys, (GCompareFunc)strcmp);

    for (auto iter = ordered_keys; iter; iter = iter->next) {
        g_print("%s\n", (const char *)iter->data);
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> OSTreeRepo::pullAll(const package::Ref &ref, bool /*force*/)
{
    // FIXME(black-desk): pullAll should not belong to this class.

    auto refs = package::Ref(QStringList{ ref.toOSTreeRefLocalString(), "runtime" }.join("/"));
    auto ret = pull(refs, false);
    if (!ret.has_value()) {
        return LINGLONG_ERR(ret.error().code(), ret.error().message());
    }

    // FIXME: some old package have no devel, ignore error for now.
    refs = package::Ref(QStringList{ ref.toOSTreeRefLocalString(), "devel" }.join("/"));
    ret = pull(refs, false);
    if (!ret.has_value()) {
        return LINGLONG_ERR(ret.error().code(), ret.error().message());
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> OSTreeRepo::checkout(const package::Ref &ref,
                                                          const QString &subPath,
                                                          const QString &target)
{
    g_autoptr(GError) gErr = NULL;
    OstreeRepoCheckoutAtOptions checkout_options = {};
    checkout_options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    checkout_options.force_copy = TRUE;
    if (!subPath.isEmpty()) {
        checkout_options.subpath = subPath.toStdString().c_str();
    }
    auto rev = resolveRev(ref.toOSTreeRefLocalString());
    if (!rev.has_value()) {
        return LINGLONG_ERR(rev.error().code(), rev.error().message());
    }

    ostree_repo_checkout_at(repoPtr.get(),
                            &checkout_options,
                            AT_FDCWD,
                            target.toStdString().c_str(),
                            (*rev).toStdString().c_str(),
                            NULL,
                            &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    return LINGLONG_OK;

    // QStringList args = { "checkout", "--union", "--force-copy" };
    // if (!subPath.isEmpty()) {
    //     args.push_back("--subpath=" + subPath);
    // }
    // args.append({ ref.toOSTreeRefLocalString(), target });
    // return ostreeRun(args);
}

linglong::utils::error::Result<void> OSTreeRepo::checkoutAll(const package::Ref &ref,
                                                             const QString &subPath,
                                                             const QString &target)
{
    // QStringList runtimeArgs = { "checkout",
    //                             "--union",
    //                             QStringList{ ref.toOSTreeRefLocalString(), "runtime" }.join("/"),
    //                             target };
    // QStringList develArgs = { "checkout",
    //                           "--union",
    //                           QStringList{ ref.toOSTreeRefLocalString(), "devel" }.join("/"),
    //                           target };

    // if (!subPath.isEmpty()) {
    //     runtimeArgs.push_back("--subpath=" + subPath);
    //     develArgs.push_back("--subpath=" + subPath);
    // }

    // auto ret = ostreeRun(runtimeArgs);
    // if (!ret.has_value()) {
    //     return LINGLONG_EWRAP("", ret.error());
    // }

    // ret = ostreeRun(develArgs);
    // if (!ret.has_value()) {
    //     return LINGLONG_EWRAP("", ret.error());
    // }
    // // Fixme: some old package have no devel, ignore error for now.
    // return LINGLONG_OK;
    g_autoptr(GError) gErr = NULL;
    OstreeRepoCheckoutAtOptions checkout_options = {};
    checkout_options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    if (!subPath.isEmpty()) {
        checkout_options.subpath = subPath.toStdString().c_str();
    }
    qInfo() << "checkoutAll:" << ref.toLocalString() + '/' + "runtime";
    auto runtimeRev = resolveRev(ref.toLocalString() + '/' + "runtime");
    if (!runtimeRev.has_value()) {
        return LINGLONG_ERR(runtimeRev.error().code(), runtimeRev.error().message());
    }
    ostree_repo_checkout_at(repoPtr.get(),
                            &checkout_options,
                            AT_FDCWD,
                            target.toStdString().c_str(),
                            (*runtimeRev).toStdString().c_str(),
                            NULL,
                            &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }

    auto develRev = resolveRev(ref.toLocalString() + '/' + "devel");
    if (!develRev.has_value()) {
        qWarning() << "failed to resolve ref with devel";
        return LINGLONG_OK;
    }
    ostree_repo_checkout_at(repoPtr.get(),
                            &checkout_options,
                            AT_FDCWD,
                            target.toStdString().c_str(),
                            (*develRev).toStdString().c_str(),
                            NULL,
                            &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }

    return LINGLONG_OK;
}

QString OSTreeRepo::rootOfLayer(const package::Ref &ref)
{
    return QStringList{ repoRootPath, "layers", ref.appId, ref.version, ref.arch }.join(
      QDir::separator());
}

linglong::utils::error::Result<QString> OSTreeRepo::remoteShowUrl(const QString &repoName)
{
    g_autofree char *out_url = nullptr;
    g_autoptr(GError) gErr = NULL;
    ostree_repo_remote_get_url(repoPtr.get(), repoName.toLocal8Bit(), &out_url, &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    return QString::fromUtf8(out_url);
}

linglong::utils::error::Result<package::Ref> OSTreeRepo::localLatestRef(const package::Ref &ref)
{

    QString latestVer = "latest";
    QString args = QString("ostree refs --repo=%1 | grep %2 | grep %3")
                     .arg(ostreePath, ref.appId, util::hostArch() + "/" + "runtime");
    qInfo() << ostreePath;
    qInfo() << args;
    qInfo() << QDir::currentPath();
    auto output = QSharedPointer<QByteArray>::create();
    auto err = util::Exec("sh", { "-c", args }, -1, output);

    if (!output) {
        return LINGLONG_ERR(-1, "output is empty");
    }

    if (!err) {
        auto outputText = QString::fromLocal8Bit(*output);
        auto lines = outputText.split('\n');
        // last line of result is null, remove it
        lines.removeLast();
        latestVer = linglong::util::latestVersion(lines);
    }

    return package::Ref("", ref.channel, ref.appId, latestVer, ref.arch, ref.module);
    // void g_hash_table_iter_init(GHashTableIter * iter, GHashTable * hash_table);
    // gboolean g_hash_table_iter_next(GHashTableIter * iter, gpointer * key, gpointer * value);

    // GHashTable **out_all_refs = nullptr;
    // g_autoptr(GError) gErr = nullptr;
    // GHashTableIter *iter = nullptr;
    // gpointer key, value;
    // ostree_repo_list_collection_refs(repoPtr,
    //                                  NULL,
    //                                  out_all_refs,
    //                                  OSTREE_REPO_LIST_REFS_EXT_NONE,
    //                                  NULL,
    //                                  &gErr);
    // if (gErr) {
    //     return LINGLONG_ERR(gErr->code, gErr->message);
    // }
    // g_hash_table_iter_init(iter, *out_all_refs);
    // while (g_hash_table_iter_next(iter, &key, &value)) {
    // }
}

package::Ref OSTreeRepo::remoteLatestRef(const package::Ref &ref)
{

    QString latestVer = "unknown";
    package::Ref queryRef(remoteRepoName, ref.appId, ref.version, ref.arch);
    auto ret = repoClient.QueryApps(queryRef);
    if (!ret.has_value()) {
        qCritical() << "query remote app failed" << ref.toSpecString() << queryRef.toSpecString();
        // FIXME: why latest version?
        return package::Ref(ref.repo, ref.channel, ref.appId, latestVer, ref.arch, ref.module);
    }

    for (const auto &info : *ret) {
        Q_ASSERT(info != nullptr);
        if (linglong::util::compareVersion(latestVer, info->version) < 0) {
            latestVer = info->version;
        }
    }

    return package::Ref(ref.repo, ref.channel, ref.appId, latestVer, ref.arch, ref.module);
}

package::Ref OSTreeRepo::latestOfRef(const QString &appId, const QString &appVersion)
{
    auto latestVersionOf = [this](const QString &appId) {
        auto localRepoRoot = QString(repoRootPath) + "/layers" + "/" + appId;

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

utils::error::Result<QString> OSTreeRepo::compressOstreeData(const package::Ref &ref)
{
    // check out ostree data
    // Fixme: use /tmp
    const auto savePath =
      QStringList{ util::getUserFile(".linglong/builder"), ref.appId }.join(QDir::separator());
    util::ensureDir(savePath);
    qInfo() << "print save path:" << savePath;

    auto ret = checkout(ref, "", savePath);
    if (!ret.has_value()) {
        return LINGLONG_ERR(-1, QString("checkout %1 to %2 failed").arg(ref.appId).arg(savePath));
    }
    // compress data
    QStringList args;
    const QString fileName = QString("%1.tgz").arg(ref.appId);
    QString filePath =
      QStringList{ util::getUserFile(".linglong/builder"), fileName }.join(QDir::separator());
    auto currentPath = QDir::currentPath();
    QDir::setCurrent(savePath);

    args << "-zcf" << filePath << ".";

    // TODO: handle error of tar
    auto error = util::Exec("tar", args);
    qDebug() << "tar with exit code:" << error.code() << error.message();
    QDir::setCurrent(currentPath);
    return { filePath };
}

/*
 * 查询 ostree 远端仓库列表
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param vec: 远端仓库列表
 * @param err: 错误信息
 *
 * @return bool: true:查询成功 false:失败
 */
linglong::utils::error::Result<void> OSTreeRepo::getRemoteRepoList(const QString &repoPath,
                                                                   QVector<QString> &vec)
{
    char info[MAX_ERRINFO_BUFSIZE] = { '\0' };
    if (repoPath.isEmpty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        // err = info;
        return LINGLONG_ERR(-1, QString(QLatin1String(info)));
    }
    // 校验本地仓库是否创建
    if (!repoPtr || repoRootPath != repoPath) {
        snprintf(info,
                 MAX_ERRINFO_BUFSIZE,
                 "%s, function:%s repo has not been created",
                 __FILE__,
                 __func__);
        // err = info;
        return LINGLONG_ERR(-1, QString(QLatin1String(info)));
    }
    g_auto(GStrv) res = nullptr;
    if (repoPtr) {
        res = ostree_repo_remote_list(repoPtr.get(), nullptr);
    }

    if (res != nullptr) {
        for (int i = 0; res[i] != nullptr; i++) {
            // vec.push_back(res[i]);
            vec.append(QLatin1String(res[i]));
        }
    } else {
        snprintf(info,
                 MAX_ERRINFO_BUFSIZE,
                 "%s, function:%s no remote repo found",
                 __FILE__,
                 __func__);
        // err = info;
        return LINGLONG_ERR(-1, QString(QLatin1String(info)));
    }

    return LINGLONG_OK;
}

/*
 * 查询远端仓库所有软件包索引信息 refs
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param remoteName: 远端仓库名称
 * @param outRefs: 远端仓库软件包索引信息 (key:refs, value:commit)
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool OSTreeRepo::getRemoteRefs(const QString &repoPath,
                               const QString &remoteName,
                               QMap<QString, QString> &outRefs,
                               QString &err)
{
    char info[MAX_ERRINFO_BUFSIZE] = { '\0' };
    if (remoteName.isEmpty()) {
        // fprintf(stdout, "getRemoteRefs param err\n");
        qInfo() << "getRemoteRefs param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!repoPtr || repoRootPath != repoPath) {
        snprintf(info,
                 MAX_ERRINFO_BUFSIZE,
                 "%s, function:%s repo has not been created",
                 __FILE__,
                 __func__);
        err = info;
        return false;
    }

    const std::string remoteNameTmp = remoteName.toStdString();
    g_autoptr(GBytes) summaryBytes = nullptr;
    g_autoptr(GBytes) summarySigBytes = nullptr;
    g_autoptr(GCancellable) cancellable = nullptr;
    g_autoptr(GError) error = nullptr;
    bool ret = fetchRemoteSummary(repoPtr.get(),
                                  remoteNameTmp.c_str(),
                                  &summaryBytes,
                                  &summarySigBytes,
                                  cancellable,
                                  &error);
    if (!ret) {
        if (err != nullptr) {
            snprintf(info,
                     MAX_ERRINFO_BUFSIZE,
                     "%s, function:%s err:%s",
                     __FILE__,
                     __func__,
                     error->message);
            err = info;
        } else {
            err = "getRemoteRefs remote repo err";
        }
        return false;
    }
    g_autoptr(GVariant) summary = g_variant_ref_sink(
      g_variant_new_from_bytes(OSTREE_SUMMARY_GVARIANT_FORMAT, summaryBytes, FALSE));
    // std::map 转 QMap
    std::map<std::string, std::string> outRet;
    getPkgRefsBySummary(summary, outRet);
    for (auto iter = outRet.begin(); iter != outRet.end(); ++iter) {
        outRefs.insert(QString::fromStdString(iter->first), QString::fromStdString(iter->second));
    }
    return true;
}

/*
 * 将软件包数据从本地仓库签出到指定目录
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param remoteName: 远端仓库名称
 * @param ref: 软件包包名对应的仓库索引
 * @param dstPath: 软件包信息保存目录
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */

linglong::utils::error::Result<void> OSTreeRepo::checkOutAppData(const QString &repoPath,
                                                                 const QString &remoteName,
                                                                 const QString &ref,
                                                                 const QString &dstPath)
{
    // // ostree --repo=repo checkout -U --union org.deepin.calculator/x86_64/1.2.2
    // // /persistent/linglong/layers/XXX
    // linglong::util::createDir(dstPath);
    // auto err =
    //   util::Exec("ostree",
    //              { "--repo=" + repoPath + "/repo", "checkout", "-U", "--union", ref, dstPath },
    //              1000 * 60 * 60 * 24);
    // if (err) {
    //     strErr = "checkOutAppData " + ref + " err";
    //     qCritical() << "checkOutAppData err, repoPath:" << repoPath << ", remoteName:" <<
    //     remoteName
    //                 << ", dstPath:" << dstPath << ", ref:" << ref;
    //     return false;
    // }

    // // FIXME(Iceyer): now we create erofs image here because no oci/linglong blobs backend
    // // implemented, it must do by vfs repo
    // QString hash(QCryptographicHash::hash(dstPath.toLocal8Bit(),
    // QCryptographicHash::Md5).toHex()); auto destImagePath =
    //   QStringList{ util::getLinglongRootPath(), "vfs", "blobs" }.join(QDir::separator());
    // util::ensureDir(destImagePath);
    // destImagePath = QStringList{ destImagePath, hash }.join(QDir::separator());
    // qDebug() << "erofs mkfs" << dstPath << destImagePath;
    // err = erofs::mkfs(dstPath, destImagePath);
    // if (err) {
    //     qCritical() << err;
    // }

    // return true;
    g_autoptr(GError) gErr = NULL;
    OstreeRepoCheckoutAtOptions checkout_options = {};
    checkout_options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    checkout_options.mode = OSTREE_REPO_CHECKOUT_MODE_USER;
    qInfo() << "checkOutAppData:" << ref;
    auto rev = resolveRev(ref);
    if (!rev.has_value()) {
        return LINGLONG_ERR(rev.error().code(), rev.error().message());
    }

    linglong::util::createDir(dstPath);
    ostree_repo_checkout_at(repoPtr.get(),
                            &checkout_options,
                            AT_FDCWD,
                            dstPath.toStdString().c_str(),
                            (*rev).toStdString().c_str(),
                            NULL,
                            &gErr);
    if (gErr) {
        return LINGLONG_ERR(rev.error().code(), rev.error().message());
    }
    // FIXME(Iceyer): now we create erofs image here because no oci/linglong blobs backend
    // implemented, it must do by vfs repo
    QString hash(QCryptographicHash::hash(dstPath.toLocal8Bit(), QCryptographicHash::Md5).toHex());
    auto destImagePath =
      QStringList{ util::getLinglongRootPath(), "vfs", "blobs" }.join(QDir::separator());
    util::ensureDir(destImagePath);
    destImagePath = QStringList{ destImagePath, hash }.join(QDir::separator());
    qDebug() << "erofs mkfs" << dstPath << destImagePath;
    auto err = erofs::mkfs(dstPath, destImagePath);
    if (err) {
        qCritical() << err;
    }

    return LINGLONG_OK;
}

/*
 * 通过 ostree 命令将软件包数据从远端仓库 pull 到本地
 *
 * @param destPath: 仓库路径
 * @param remoteName: 远端仓库名称
 * @param ref: 软件包对应的仓库索引 ref
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
linglong::utils::error::Result<void> OSTreeRepo::repoPullbyCmd(const QString &destPath,
                                                               const QString &remoteName,
                                                               const QString &ref)
{
    // 创建临时仓库
    auto tmpPath = createTmpRepo(destPath + "/repo");
    if (!tmpPath.has_value()) {
        return LINGLONG_ERR(-1, "create tmp repo err");
    }

    g_autoptr(GError) gErr = nullptr;
    std::string refString = ref.toStdString();
    char *refs = (char *)refString.c_str();
    char *refs_to_fetch[2] = { refs, nullptr };
    g_autoptr(GFile) src_repo_path = g_file_new_for_path((*tmpPath).toStdString().c_str());
    g_autoptr(OstreeRepo) tmpRepo = ostree_repo_new(src_repo_path);
    auto remote = remoteRepoName.toStdString();

    g_autoptr(GVariant) options = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "http2", g_variant_new_boolean(false));
    g_variant_builder_add(&builder, "{sv}", "gpg-verify", g_variant_new_boolean(false));
    options = g_variant_ref_sink(g_variant_builder_end(&builder));
    if (ostree_repo_open(tmpRepo, NULL, &gErr)) {
        ostree_repo_remote_add(tmpRepo,
                               remoteName.toStdString().c_str(),
                               (remoteEndpoint + "/repos/repo").toStdString().c_str(),
                               options,
                               NULL,
                               &gErr);
        if (gErr) {
            return LINGLONG_ERR(gErr->code, gErr->message);
        }
    }
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }

    ostree_repo_pull(tmpRepo,
                     remote.c_str(),
                     refs_to_fetch,
                     OSTREE_REPO_PULL_FLAGS_MIRROR,
                     NULL,
                     NULL,
                     &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    qInfo() << "repoPullbyCmd pull success";

    auto path = destPath + "/repo";
    auto dest_repo_path = g_file_new_for_path(path.toStdString().c_str());
    g_autoptr(OstreeRepo) repoDest = ostree_repo_new(dest_repo_path);
    if (!ostree_repo_open(repoDest, NULL, &gErr)) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    g_autofree auto base_url = g_strconcat("file://", tmpPath->toStdString().c_str(), NULL);
    builder = {};
    options = NULL;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(
      &builder,
      "{s@v}",
      "refs",
      g_variant_new_variant(g_variant_new_strv((const char *const *)refs_to_fetch, -1)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-static-deltas",
                          g_variant_new_variant(g_variant_new_boolean(true)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify-summary",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));

    options = g_variant_ref_sink(g_variant_builder_end(&builder));

    ostree_repo_pull_with_options(repoDest, (char *)base_url, options, NULL, NULL, &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }
    // 删除临时仓库
    QString tmpRepoDir = (*tmpPath).left((*tmpPath).length() - QString("/repoTmp").length());
    qInfo() << "delete tmp repo path:" << tmpRepoDir;
    linglong::util::removeDir(tmpRepoDir);
    return LINGLONG_OK;
}

/*
 * 删除本地 repo 仓库中软件包对应的 ref 分支信息及数据
 *
 * @param repoPath: 仓库路径
 * @param remoteName: 远端仓库名称
 * @param ref: 软件包对应的仓库索引 ref
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
linglong::utils::error::Result<void> OSTreeRepo::repoDeleteDatabyRef(const QString &repoPath,
                                                                     const QString &ref)
{
    if (repoPath.isEmpty() || ref.isEmpty()) {
        return LINGLONG_ERR(-1, "repoDeleteDatabyRef param error");
    }
    g_autoptr(GError) gErr = nullptr;
    auto rev = resolveRev(ref);
    if (!rev.has_value()) {
        return LINGLONG_ERR(rev.error().code(), rev.error().message());
    }

    if (!ostree_repo_prune_static_deltas(repoPtr.get(),
                                         (*rev).toStdString().c_str(),
                                         nullptr,
                                         &gErr)) {
        return LINGLONG_ERR(gErr->code, gErr->message);
    }

    // const std::string refTmp = ref.toStdString();
    // g_autoptr(GCancellable) cancellable = nullptr;
    // g_autoptr(GError) error = nullptr;

    // if (!ostree_repo_set_ref_immediate(repoPtr,
    //                                    nullptr,
    //                                    refTmp.c_str(),
    //                                    nullptr,
    //                                    cancellable,
    //                                    &error)) {
    //     return LINGLONG_ERR(-1,
    //                         "repoDeleteDatabyRef error:" +
    //                         QString(QLatin1String(error->message)));
    // }
    // qInfo() << "repoDeleteDatabyRef delete " << refTmp.c_str() << " success";

    // gint objectsTotal;
    // gint objectsPruned;
    // guint64 objsizeTotal;
    // g_autofree char *formattedFreedSize = NULL;
    // if (!ostree_repo_prune(repoPtr,
    //                        OSTREE_REPO_PRUNE_FLAGS_REFS_ONLY,
    //                        0,
    //                        &objectsTotal,
    //                        &objectsPruned,
    //                        &objsizeTotal,
    //                        cancellable,
    //                        &error)) {
    //     return LINGLONG_ERR(-1,
    //                         "repoDeleteDatabyRef pruning repo failed:"
    //                           + QString(QLatin1String(error->message)));
    // }

    // formattedFreedSize = g_format_size_full(objsizeTotal, (GFormatSizeFlags)0);
    // qInfo() << "repoDeleteDatabyRef Total objects:" << objectsTotal;
    // if (objectsPruned == 0) {
    //     qInfo() << "repoDeleteDatabyRef No unreachable objects";
    // } else {
    //     qInfo() << "Deleted " << objectsPruned << " objects," << formattedFreedSize << " freed";
    // }

    // const QString fullref = remoteName + ":" + ref;
    // auto ret = Runner("ostree", {"--repo=" + repoPath + "/repo", "refs", "--delete", ref}, 1000 *
    // 60 * 30); if (!ret) {
    //     qInfo() << "repoDeleteDatabyRef delete ref error";
    //     err = "repoDeleteDatabyRef delete ref error";
    //     return false;
    // }
    // ret = Runner("ostree", {"--repo=" + repoPath + "/repo", "prune", "--refs-only"}, 1000 * 60 *
    // 30); if (!ret) {
    //     qInfo() << "repoDeleteDatabyRef prune data error";
    //     err = "repoDeleteDatabyRef prune data error";
    //     return false;
    // }
    // qInfo() << "repoDeleteDatabyRef delete " << ref << " success";
    return LINGLONG_OK;
}

/*
 * 查询远端 ostree 仓库描述文件 Summary 信息
 *
 * @param repo: 远端仓库对应的本地仓库 OstreeRepo 对象
 * @param name: 远端仓库名称
 * @param outSummary: 远端仓库的 Summary 信息
 * @param outSummarySig: 远端仓库的 Summary 签名信息
 * @param cancellable: GCancellable 对象
 * @param error: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool OSTreeRepo::fetchRemoteSummary(OstreeRepo *repo,
                                    const char *name,
                                    GBytes **outSummary,
                                    GBytes **outSummarySig,
                                    GCancellable *cancellable,
                                    GError **error)
{
    g_autofree char *url = nullptr;
    g_autoptr(GBytes) summary = nullptr;
    g_autoptr(GBytes) summarySig = nullptr;

    if (!ostree_repo_remote_get_url(repo, name, &url, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_get_url error:%s\n",
        // (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_get_url error:" << (*error)->message;
        return false;
    }
    // fprintf(stdout, "fetchRemoteSummary remote %s,url:%s\n", name, url);

    if (!ostree_repo_remote_fetch_summary(repo, name, &summary, &summarySig, cancellable, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_fetch_summary error:%s\n",
        // (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_fetch_summary error:"
                << (*error)->message;
        return false;
    }

    if (summary == nullptr) {
        // fprintf(stdout, "fetch summary error");
        qInfo() << "fetch summary error";
        return false;
    }
    *outSummary = (GBytes *)g_steal_pointer(&summary);
    if (outSummarySig)
        *outSummarySig = (GBytes *)g_steal_pointer(&summarySig);
    return true;
}

/*
 * 从 summary 中的 refMap 中获取仓库所有软件包索引 refs
 *
 * @param ref_map: summary 信息中解析出的 ref map 信息
 * @param outRefs: 仓库软件包索引信息
 */
void OSTreeRepo::getPkgRefsFromRefsMap(GVariant *ref_map,
                                       std::map<std::string, std::string> &outRefs)
{
    GVariant *value;
    GVariantIter ref_iter;
    g_variant_iter_init(&ref_iter, ref_map);
    while ((value = g_variant_iter_next_value(&ref_iter)) != nullptr) {
        /* helper for being able to auto-free the value */
        g_autoptr(GVariant) child = value;
        g_autofree char *ref_name = nullptr;
        g_variant_get_child(child, 0, "&s", &ref_name);
        if (ref_name == nullptr)
            continue;
        g_autofree char *ref = nullptr;
        ostree_parse_refspec(ref_name, nullptr, &ref, nullptr);
        if (ref == nullptr)
            continue;
        // gboolean is_app = g_str_has_prefix(ref, "app/");

        g_autoptr(GVariant) csum_v = nullptr;
        char tmp_checksum[65];
        g_autofree const guchar *csum_bytes;
        g_variant_get_child(child, 1, "(t@aya{sv})", nullptr, &csum_v, nullptr);
        csum_bytes = ostree_checksum_bytes_peek_validate(csum_v, nullptr);
        if (csum_bytes == nullptr)
            continue;
        ostree_checksum_inplace_from_bytes(csum_bytes, tmp_checksum);
        // char *newRef = g_new0(char, 1);
        // char* newRef = NULL;
        // newRef = g_strdup(ref);
        // g_hash_table_insert(ret_all_refs, newRef, g_strdup(tmp_checksum));
        outRefs.insert(std::map<std::string, std::string>::value_type(ref, tmp_checksum));
    }
}

/*
 * 从 ostree 仓库描述文件 Summary 信息中获取仓库所有软件包索引 refs
 *
 * @param summary: 远端仓库 Summary 信息
 * @param outRefs: 远端仓库软件包索引信息
 */
void OSTreeRepo::getPkgRefsBySummary(GVariant *summary, std::map<std::string, std::string> &outRefs)
{
    // g_autoptr(GHashTable) ret_all_refs = NULL;
    g_autoptr(GVariant) ref_map = nullptr;
    // g_autoptr(GVariant) metadata = NULL;

    // ret_all_refs = g_hash_table_new(linglong_collection_ref_hash, linglong_collection_ref_equal);
    ref_map = g_variant_get_child_value(summary, 0);
    // metadata = g_variant_get_child_value(summary, 1);
    getPkgRefsFromRefsMap(ref_map, outRefs);
}

/*
 * 解析仓库软件包索引 ref 信息
 *
 * @param fullRef: 目标软件包索引 ref 信息
 * @param result: 解析结果
 *
 * @return bool: true:成功 false:失败
 */
bool OSTreeRepo::resolveRef(const std::string &fullRef, std::vector<std::string> &result)
{
    // vector<string> result;
    splitStr(fullRef, "/", result);
    // new ref format org.deepin.calculator/1.2.2/x86_64
    if (result.size() != 3) {
        qCritical() << "resolveRef Wrong number of components err";
        return false;
    }

    return true;
}

/*
 * 在玲珑应用安装目录创建一个临时 repo 子仓库
 *
 * @param parentRepo: 父 repo 仓库路径
 *
 * @return QString: 临时 repo 路径
 */
linglong::utils::error::Result<QString> OSTreeRepo::createTmpRepo(const QString &parentRepo)
{
    QString baseDir = linglong::util::getLinglongRootPath() + "/.cache";
    linglong::util::createDir(baseDir);
    QTemporaryDir dir(baseDir + "/linglong-cache-XXXXXX");
    QString tmpPath;
    if (dir.isValid()) {
        tmpPath = dir.path();
    } else {
        qCritical() << "create tmpPath failed, please check " << baseDir << ","
                    << dir.errorString();
        return QString();
    }
    dir.setAutoRemove(false);
    tmpPath += "/repoTmp";
    g_autoptr(GFile) file = g_file_new_for_path(tmpPath.toLocal8Bit());
    g_autoptr(OstreeRepo) tmpRepo = ostree_repo_new(file);
    g_autoptr(GError) gErr = nullptr;
    ostree_repo_create(tmpRepo, OSTREE_REPO_MODE_BARE_USER_ONLY, NULL, &gErr);
    if (gErr) {
        return LINGLONG_ERR(-1, QString("init repoTmp failed: %1").arg(gErr->message));
    }
    // if (!ostree_repo_open(tmpRepo, nullptr, &gErr))
    //     return LINGLONG_ERR(gErr->code, gErr->message);

    g_autoptr(GKeyFile) config = ostree_repo_copy_config(tmpRepo);
    if (config == nullptr) {
        return LINGLONG_ERR(-1, "failed to get repo config");
    }
    g_key_file_set_string(config, "core", "min-free-space-size", "600MB");

    // // 设置最小空间要求
    // auto err = util::Exec("ostree",
    //                       { "config",
    //                         "set",
    //                         "--group",
    //                         "core",
    //                         "min-free-space-size",
    //                         "600MB",
    //                         "--repo",
    //                         tmpPath + "/repoTmp" },
    //                       1000 * 60 * 5);
    ostree_repo_write_config(tmpRepo, config, &gErr);
    if (gErr) {
        return LINGLONG_ERR(
          gErr->code,
          QString("config set min-free-space-size failed: %1").arg(gErr->message));
    }
    // 添加父仓库路径
    g_key_file_set_string(config, "core", "parent", parentRepo.toStdString().c_str());
    ostree_repo_write_config(tmpRepo, config, &gErr);
    if (gErr) {
        return LINGLONG_ERR(gErr->code, "config set parent failed");
    }

    qInfo() << "create tmp repo path:" << tmpPath << ", ret:" << QDir().exists(tmpPath);
    return tmpPath;
}

// NOTE(black_desk):
// This function is used to called in OSTreeRepo's constructor,
// to make sure the ostree-based linglong repository we using exists.
linglong::utils::error::Result<void> OSTreeRepo::initCreateRepoIfNotExists()
{
    Q_ASSERT(!repoPtr);

    Q_ASSERT(!repoRootPath.isEmpty());

    QString repoPath = repoRootPath + "/repo";
    if (!QDir(repoPath).mkpath(".")) {
        return LINGLONG_ERR(-1, "failed create directory " + repoPath);
    }

    g_autoptr(GFile) repodir = g_file_new_for_path(repoPath.toLocal8Bit());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(repodir);
    g_autoptr(GError) gErr = nullptr;

    if (!ostree_repo_open(repo, nullptr, &gErr)) {
        qDebug() << "ostree_repo_open failed:" << gErr->message << "[ code" << gErr->code << "]";
        g_clear_error(&gErr);
        g_clear_object(&repo);
        repo = ostree_repo_new(repodir);

        qInfo() << "Creating linglong ostree repo at" << repoPath;
        if (!ostree_repo_create(repo, OSTREE_REPO_MODE_BARE_USER_ONLY, nullptr, &gErr)) {
            return LINGLONG_ERR(-1, "ostree_repo_create error:" + QLatin1String(gErr->message));
        }
    }

    QString url = this->remoteEndpoint;

    url += "/repos/" + this->remoteRepoName;

    g_clear_error(&gErr);
    if (!ostree_repo_remote_change(repo,
                                   nullptr,
                                   OSTREE_REPO_REMOTE_CHANGE_REPLACE,
                                   this->remoteRepoName.toLocal8Bit(),
                                   url.toLocal8Bit(),
                                   nullptr,
                                   nullptr,
                                   &gErr)) {
        return LINGLONG_ERR(-1,
                            QString("Failed to add remote repo [%1 %2], message: %3")
                              .arg(this->remoteRepoName, this->remoteEndpoint, gErr->message));
    }

    GKeyFile *configKeyFile = ostree_repo_get_config(repo);
    if (!configKeyFile) {
        return LINGLONG_ERR(-1, QString("Failed to get config of repo"));
    }

    g_key_file_set_string(configKeyFile, "core", "min-free-space-size", "600MB");
    // NOTE:
    // libcurl 8.2.1 has a http2 bug https://github.com/curl/curl/issues/11859
    // We disable http2 for now.
    g_key_file_set_string(configKeyFile,
                          QString("remote \"%1\"").arg(remoteRepoName).toLocal8Bit(),
                          "http2",
                          "false");
    g_key_file_set_string(configKeyFile,
                          QString("remote \"%1\"").arg(remoteRepoName).toLocal8Bit(),
                          "gpg-verify",
                          "false");

    g_clear_error(&gErr);
    if (!ostree_repo_write_config(repo, configKeyFile, &gErr)) {
        return LINGLONG_ERR(-1, QString("Failed to write config, message:%1").arg(gErr->message));
    }

    qDebug() << "new OstreeRepo" << repo;

    this->repoPtr.reset(g_steal_pointer(&repo));

    return LINGLONG_OK;
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace repo
} // namespace linglong
