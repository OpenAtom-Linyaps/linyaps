/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_

#include "linglong/package/package.h"
#include "linglong/package/ref.h"
#include "linglong/repo/repo.h"
#include "linglong/repo/repo_client.h"
#include "linglong/util/erofs.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <QHttpPart>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QScopedPointer>
#include <QThread>

namespace linglong {
namespace repo {

class RevPair : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(RevPair)

    Q_JSON_PROPERTY(QString, server);
    Q_JSON_PROPERTY(QString, client);
};

class UploadResponseData : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadResponseData)

    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, watchId);
    Q_JSON_PROPERTY(QString, status);
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, RevPair)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadResponseData)

namespace linglong {
namespace repo {

class UploadTaskRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadTaskRequest)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QStringList, objects);
    Q_PROPERTY(QMap<QString, QSharedPointer<linglong::repo::RevPair>> refs MEMBER refs);
    QMap<QString, QSharedPointer<linglong::repo::RevPair>> refs;
};

class UploadRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadRequest)

    Q_JSON_PROPERTY(QString, repoName);
    Q_JSON_PROPERTY(QString, ref);
};

class UploadTaskResponse : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadTaskResponse)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QString, msg);
    Q_JSON_PTR_PROPERTY(linglong::repo::UploadResponseData, data);
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadRequest)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadTaskRequest)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadTaskResponse)

namespace linglong {
namespace repo {
// ostree 仓库对象信息

struct OstreeRepoObject
{
    QString objectName;
    QString rev;
    QString path;
};

class OSTreeRepo : public QObject, public Repo
{
    Q_OBJECT
public:
    enum Tree : quint32 {
        DIR,
        TAR,
        REF,
    };

    explicit OSTreeRepo(const QString &localRepoPath,
                        const config::ConfigV1 &cfg,
                        api::client::ClientApi &client);

    ~OSTreeRepo() override;

    config::ConfigV1 getConfig() const noexcept override;
    linglong::utils::error::Result<void> setConfig(const config::ConfigV1 &cfg) noexcept override;

    linglong::utils::error::Result<void> listRemoteRefs();
    linglong::utils::error::Result<QList<package::Ref>> listLocalRefs() noexcept override;

    linglong::utils::error::Result<void> importRef(const package::Ref &oldRef,
                                                   const package::Ref &newRef);

    linglong::utils::error::Result<void> importDirectory(const package::Ref &ref,
                                                         const QString &path) override;

    linglong::utils::error::Result<void> commit(const Tree treeType,
                                                const package::Ref &ref,
                                                const QString &path,
                                                const package::Ref &oldRef);

    linglong::utils::error::Result<void> push(const package::Ref &ref) override;

    linglong::utils::error::Result<void> pull(package::Ref &ref, bool force) override;

    linglong::utils::error::Result<void> pullAll(const package::Ref &ref, bool force) override;

    linglong::utils::error::Result<void> checkout(const package::Ref &ref,
                                                  const QString &subPath,
                                                  const QString &target) override;

    linglong::utils::error::Result<void> checkoutAll(const package::Ref &ref,
                                                     const QString &subPath,
                                                     const QString &target) override;

    linglong::utils::error::Result<QString> compressOstreeData(const package::Ref &ref);

    QString rootOfLayer(const package::Ref &ref) override;

    linglong::utils::error::Result<QString> remoteShowUrl(const QString &repoName) override;

    linglong::utils::error::Result<package::Ref> localLatestRef(const package::Ref &ref) override;

    linglong::utils::error::Result<package::Ref> remoteLatestRef(const package::Ref &ref) override;

    utils::error::Result<package::Ref> latestOfRef(const QString &appId,
                                                   const QString &appVersion) override;

    /*
     * 查询ostree远端仓库列表
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param vec: 远端仓库列表
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    linglong::utils::error::Result<void> getRemoteRepoList(QVector<QString> &vec) override;

    /*
     * 通过ostree命令将软件包数据从远端仓库pull到本地
     *
     * @param destPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    linglong::utils::error::Result<void> repoPullbyCmd(const QString &destPath,
                                                       const QString &remoteName,
                                                       const QString &ref) override;

    /*
     * 删除本地repo仓库中软件包对应的ref分支信息及数据
     *
     * @param repoPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    linglong::utils::error::Result<void> repoDeleteDatabyRef(const QString &repoPath,
                                                             const QString &ref) override;

    linglong::utils::error::Result<void> initCreateRepoIfNotExists();

    /*
     * 获取下载任务对应的进程Id
     *
     * @param ref: ostree软件包对应的ref
     *
     * @return int: ostree命令任务对应的进程id
     */
    int getOstreeJobId(const QString &ref)
    {
        if (jobMap.contains(ref)) {
            return jobMap[ref];
        } else {
            for (auto item : jobMap.keys()) {
                if (item.indexOf(ref) > -1) {
                    return jobMap[item];
                }
            }
        }
        return -1;
    }

    /*
     * 获取正在下载的任务列表
     *
     * @return QStringList: 正在下载的应用对应的ref列表
     */
    QStringList getOstreeJobList()
    {
        if (jobMap.isEmpty()) {
            return {};
        }
        return jobMap.keys();
    }

private:
    /*
     * 查询远端ostree仓库描述文件Summary信息
     *
     * @param repo: 远端仓库对应的本地仓库OstreeRepo对象
     * @param name: 远端仓库名称
     * @param outSummary: 远端仓库的Summary信息
     * @param outSummarySig: 远端仓库的Summary签名信息
     * @param cancellable: GCancellable对象
     * @param error: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool fetchRemoteSummary(OstreeRepo *repo,
                            const char *name,
                            GBytes **outSummary,
                            GBytes **outSummarySig,
                            GCancellable *cancellable,
                            GError **error);

    /*
     * 从ostree仓库描述文件Summary信息中获取仓库所有软件包索引refs
     *
     * @param summary: 远端仓库Summary信息
     * @param outRefs: 远端仓库软件包索引信息
     */
    void getPkgRefsBySummary(GVariant *summary, std::map<std::string, std::string> &outRefs);

    /*
     * 从summary中的refMap中获取仓库所有软件包索引refs
     *
     * @param ref_map: summary信息中解析出的ref map信息
     * @param outRefs: 仓库软件包索引信息
     */
    void getPkgRefsFromRefsMap(GVariant *ref_map, std::map<std::string, std::string> &outRefs);

    /*
     * 解析仓库软件包索引ref信息
     *
     * @param fullRef: 目标软件包索引ref信息
     * @param result: 解析结果
     *
     * @return bool: true:成功 false:失败
     */
    bool resolveRef(const std::string &fullRef, std::vector<std::string> &result);

    /*
     * 在/tmp目录下创建一个临时repo子仓库
     *
     * @param parentRepo: 父repo仓库路径
     *
     * @return QString: 临时repo路径
     */
    linglong::utils::error::Result<QString> createTmpRepo(const QString &parentRepo);

    QString getObjectPath(const QString &objName)
    {
        return QDir::cleanPath(
          QStringList{ ostreePath, "objects", objName.left(2), objName.right(objName.length() - 2) }
            .join(QDir::separator()));
    }

    // FIXME: return {Error, QStringList}
    QStringList traverseCommit(const QString &rev, int maxDepth)
    {
        g_autoptr(GHashTable) hashTable = nullptr;
        g_autoptr(GError) gErr = nullptr;
        QStringList objects;

        std::string str = rev.toStdString();

        if (!ostree_repo_traverse_commit(repoPtr.get(),
                                         str.c_str(),
                                         maxDepth,
                                         &hashTable,
                                         nullptr,
                                         &gErr)) {
            qCritical() << "ostree_repo_traverse_commit failed"
                        << "rev" << rev << QString::fromStdString(std::string(gErr->message));
            return {};
        }

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, hashTable);

        g_autoptr(GVariant) object;
        for (; g_hash_table_iter_next(&iter, reinterpret_cast<gpointer *>(&object), nullptr);) {
            g_autofree char *checksum;
            OstreeObjectType objectType;

            // TODO: check error
            g_variant_get(object, "(su)", &checksum, &objectType);
            QString objectName(ostree_object_to_string(checksum, objectType));
            objects.push_back(objectName);
        }

        return objects;
    }

    linglong::utils::error::Result<QString> resolveRev(const QString &ref)
    {
        LINGLONG_TRACE(QString("resolve refspec %1").arg(ref));

        g_autoptr(GError) gErr = nullptr;
        g_autofree char *commitID = nullptr;

        if (ostree_repo_resolve_rev(repoPtr.get(), ref.toUtf8(), 0, &commitID, &gErr) == 0) {
            return LINGLONG_ERR("ostree_repo_resolve_rev", gErr);
        }

        return QString::fromUtf8(commitID);
    }

    linglong::utils::error::Result<QSharedPointer<api::client::GetRepo_200_response>>
    getRepoInfo(const QString &repoName)
    {
        LINGLONG_TRACE("get repo");

        linglong::utils::error::Result<QSharedPointer<api::client::GetRepo_200_response>> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::getRepoSignal,
          &loop,
          [&](const api::client::GetRepo_200_response &resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }

              ret = QSharedPointer<api::client::GetRepo_200_response>::create(resp);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::getRepoSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        apiClient.getRepo(repoName);
        loop.exec();

        return ret;
    }

    linglong::utils::error::Result<QString> getToken()
    {
        LINGLONG_TRACE("get token");

        linglong::utils::error::Result<QString> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::signInSignal,
          &loop,
          [&](api::client::SignIn_200_response resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              ret = resp.getData().getToken();
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::signInSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        // get username and password from environment
        auto env = QProcessEnvironment::systemEnvironment();
        api::client::Request_Auth auth;
        auth.setUsername(env.value("LINGLONG_USERNAME"));
        auth.setPassword(env.value("LINGLONG_PASSWORD"));
        qDebug() << auth.asJson();
        apiClient.signIn(auth);
        loop.exec();
        return ret;
    }

    linglong::utils::error::Result<QString> newUploadTask(QSharedPointer<UploadRequest> req)
    {
        LINGLONG_TRACE("new upload task");

        linglong::utils::error::Result<QString> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::newUploadTaskIDSignal,
          &loop,
          [&](api::client::NewUploadTaskID_200_response resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              ret = resp.getData().getId();
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::newUploadTaskIDSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        api::client::Schema_NewUploadTaskReq taskReq;
        taskReq.setRef(req->ref);
        taskReq.setRepoName(req->repoName);
        apiClient.newUploadTaskID(remoteToken, taskReq);
        loop.exec();
        return ret;
    }

    linglong::utils::error::Result<void> doUploadTask(const QString &taskID,
                                                      const QString &filePath)
    {
        LINGLONG_TRACE("do upload task");

        linglong::utils::error::Result<void> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::uploadTaskFileSignal,
          &loop,
          [&](const api::client::Api_UploadTaskFileResp &resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::uploadTaskFileSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        api::client::HttpFileElement file;
        file.setFileName(filePath);
        file.setRequestFileName(filePath);
        apiClient.uploadTaskFile(remoteToken, taskID, file);
        loop.exec();
        return ret;
    }

    static void cleanUploadTask(const package::Ref &ref, const QString &filePath)
    {
        const auto savePath =
          QStringList{ util::getUserFile(".linglong/builder"), ref.appId }.join(QDir::separator());

        if (!util::removeDir(savePath)) {
            Q_ASSERT(false);
            qCritical() << "Failed to remove" << savePath;
        }

        QFile file(filePath);
        if (!file.remove()) {
            Q_ASSERT(false);
            qCritical() << "Failed to remove" << filePath;
        }
    }

    linglong::utils::error::Result<void> getUploadStatus(const QString &taskID)
    {
        LINGLONG_TRACE("get upload status");

        while (true) {
            linglong::utils::error::Result<QString> ret = LINGLONG_OK;
            qDebug() << "OK have value" << ret.has_value();
            QEventLoop loop;
            QEventLoop::connect(
              &apiClient,
              &api::client::ClientApi::uploadTaskInfoSignal,
              &loop,
              [&](const api::client::UploadTaskInfo_200_response &resp) {
                  loop.exit();
                  const qint32 HTTP_OK = 200;
                  if (resp.getCode() != HTTP_OK) {
                      ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                      return;
                  }
                  ret = resp.getData().getStatus();
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);

            QEventLoop::connect(
              &apiClient,
              &api::client::ClientApi::uploadTaskInfoSignalEFull,
              &loop,
              [&](auto, auto error_type, const QString &error_str) {
                  loop.exit();
                  ret = LINGLONG_ERR(error_str, error_type);
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);
            apiClient.uploadTaskInfo(remoteToken, taskID);
            loop.exec();
            if (!ret) {
                return LINGLONG_ERR("get upload taks info", ret);
            }
            auto status = *ret;
            if (status == "complete") {
                break;
            } else if (status == "failed") {
                return LINGLONG_ERR("task status failed", -1);
            }

            qInfo().noquote() << status;
            QThread::sleep(1);
        }

        return LINGLONG_OK;
    }

    static char *_formatted_time_remaining_from_seconds(guint64 seconds_remaining);

    static void progress_changed(OstreeAsyncProgress *progress, gpointer user_data);

Q_SIGNALS:
    void progressChanged(const uint &progress, const QString &speed);

private:
    config::ConfigV1 cfg;
    QString repoRootPath;
    QString remoteEndpoint;
    QString remoteRepoName;

    QString remoteToken;

    struct OstreeRepoDeleter
    {
        void operator()(OstreeRepo *repo)
        {
            qDebug() << "delete OstreeRepo" << repo;
            g_clear_object(&repo);
        }
    };

    std::unique_ptr<OstreeRepo, OstreeRepoDeleter> repoPtr = nullptr;
    QString ostreePath;

    repo::RepoClient repoClient;
    api::client::ClientApi &apiClient;
    // ostree 仓库对象信息

    QMap<QString, int> jobMap;
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
