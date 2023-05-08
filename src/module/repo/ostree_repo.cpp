/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "module/package/info.h"
#include "module/package/ref.h"
#include "module/repo/ostree_repohelper.h"
#include "module/util/error.h"
#include "module/util/file.h"
#include "module/util/http/http_client.h"
#include "module/util/http/httpclient.h"
#include "module/util/qserializer/json.h"
#include "module/util/runner.h"
#include "module/util/sysinfo.h"
#include "module/util/version/semver.h"
#include "module/util/version/version.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <QDir>
#include <QHttpPart>
#include <QProcess>
#include <QThread>
#include <QtWebSockets/QWebSocket>

#include <utility>

namespace linglong {
namespace repo {

QSERIALIZER_IMPL(InfoResponse);
QSERIALIZER_IMPL(RevPair);
QSERIALIZER_IMPL(UploadResponseData);
QSERIALIZER_IMPL(AuthResponseData);

QSERIALIZER_IMPL(UploadTaskRequest);
QSERIALIZER_IMPL(UploadRequest);
QSERIALIZER_IMPL(UploadTaskResponse);
QSERIALIZER_IMPL(AuthResponse);

struct OstreeRepoObject
{
    QString objectName;
    QString rev;
    QString path;
};

typedef QMap<QString, OstreeRepoObject> RepoObjectMap;

class OSTreeRepoPrivate
{
public:
    ~OSTreeRepoPrivate()
    {
        if (repoPtr) {
            g_object_unref(repoPtr);
            repoPtr = nullptr;
        }
    };

private:
    OSTreeRepoPrivate(QString localRepoRootPath,
                      QString remoteEndpoint,
                      QString remoteRepoName,
                      OSTreeRepo *parent)
        : repoRootPath(std::move(localRepoRootPath))
        , remoteEndpoint(std::move(remoteEndpoint))
        , remoteRepoName(std::move(remoteRepoName))
        , q_ptr(parent)
    {
        QString repoCreateErr;
        // FIXME(black_desk): Just a quick hack to make sure openRepo called after the repo is
        // created. So I am not to check error here.
        OSTREE_REPO_HELPER->ensureRepoEnv(localRepoRootPath, repoCreateErr);
        // FIXME: should be repo
        if (QDir(repoRootPath).exists("/repo/repo")) {
            ostreePath = repoRootPath + "/repo/repo";
        } else {
            ostreePath = repoRootPath + "/repo";
        }
        qDebug() << "ostree repo path is" << ostreePath;

        repoPtr = openRepo(ostreePath);
    }

    linglong::util::Error ostreeRun(const QStringList &args, QByteArray *stdout = nullptr)
    {
        QProcess ostree;
        ostree.setProgram("ostree");

        QStringList ostreeArgs = { "-v", "--repo=" + ostreePath };
        ostreeArgs.append(args);
        ostree.setArguments(ostreeArgs);

        QProcess::connect(&ostree, &QProcess::readyReadStandardOutput, [&]() {
            // ostree.readAllStandardOutput() can only be read once.
            if (stdout) {
                *stdout += ostree.readAllStandardOutput();
                qDebug() << QString::fromLocal8Bit(*stdout);
            } else {
                qDebug() << QString::fromLocal8Bit(ostree.readAllStandardOutput());
            }
        });

        qDebug() << "start" << ostree.arguments().join(" ");
        ostree.start();
        ostree.waitForFinished(-1);
        qDebug() << ostree.exitStatus() << "with exit code:" << ostree.exitCode();

        return NewError(ostree.exitCode(), QString::fromLocal8Bit(ostree.readAllStandardError()));
    }

    static QByteArray glibBytesToQByteArray(GBytes *bytes)
    {
        const void *data;
        gsize length;
        data = g_bytes_get_data(bytes, &length);
        return { reinterpret_cast<const char *>(data), static_cast<int>(length) };
    }

    static GBytes *glibInputStreamToBytes(GInputStream *inputStream)
    {
        g_autoptr(GOutputStream) outStream = nullptr;
        g_autoptr(GError) gErr = nullptr;

        if (inputStream == nullptr)
            return g_bytes_new(nullptr, 0);

        outStream = g_memory_output_stream_new(nullptr, 0, g_realloc, g_free);
        g_output_stream_splice(outStream,
                               inputStream,
                               G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                               nullptr,
                               &gErr);
        g_assert_no_error(gErr);

        return g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(outStream));
    }

    // FIXME: use tmp path for big file.
    // FIXME: free the glib pointer.
    static std::tuple<util::Error, QByteArray> compressFile(const QString &filepath)
    {
        g_autoptr(GError) gErr = nullptr;
        g_autoptr(GBytes) inputBytes = nullptr;
        g_autoptr(GInputStream) memInput = nullptr;
        g_autoptr(GInputStream) zlibStream = nullptr;
        g_autoptr(GBytes) zlibBytes = nullptr;
        g_autoptr(GFile) file = nullptr;
        g_autoptr(GFileInfo) info = nullptr;
        g_autoptr(GVariant) xattrs = nullptr;

        file = g_file_new_for_path(filepath.toStdString().c_str());
        if (file == nullptr) {
            return { NewError(errno, "open file failed: " + filepath), QByteArray() };
        }

        info = g_file_query_info(file,
                                 G_FILE_ATTRIBUTE_UNIX_MODE,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 nullptr,
                                 nullptr);
        guint32 mode = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE);

        info = g_file_query_info(file,
                                 G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 nullptr,
                                 nullptr);
        if (g_file_info_get_is_symlink(info)) {
            info = g_file_query_info(file,
                                     G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     nullptr,
                                     nullptr);
            g_file_info_set_file_type(info, G_FILE_TYPE_SYMBOLIC_LINK);
            g_file_info_set_size(info, 0);
        } else {
            info = g_file_query_info(file,
                                     G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                     nullptr,
                                     nullptr);
            qDebug() << "fize size:" << g_file_info_get_size(info);

            // Q_ASSERT(g_file_info_get_size(info) > 0);
            g_file_info_set_size(info, g_file_info_get_size(info));
            inputBytes = g_file_load_bytes(file, nullptr, nullptr, nullptr);
        }
        // TODO: set uid/gid with G_FILE_ATTRIBUTE_UNIX_UID/G_FILE_ATTRIBUTE_UNIX_GID
        g_file_info_set_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE, mode);

        if (inputBytes) {
            memInput = g_memory_input_stream_new_from_bytes(inputBytes);
        }

        xattrs = g_variant_ref_sink(g_variant_new_array(G_VARIANT_TYPE("(ayay)"), nullptr, 0));

        ostree_raw_file_to_archive_z2_stream(memInput, info, xattrs, &zlibStream, nullptr, &gErr);

        zlibBytes = glibInputStreamToBytes(zlibStream);

        return { Success(), glibBytesToQByteArray(zlibBytes) };
    }

    static OstreeRepo *openRepo(const QString &path)
    {
        GError *gErr = nullptr;
        std::string repoPathStr = path.toStdString();

        auto repoPath = g_file_new_for_path(repoPathStr.c_str());
        auto repo = ostree_repo_new(repoPath);
        if (!ostree_repo_open(repo, nullptr, &gErr)) {
            qCritical() << "open repo" << path << "failed"
                        << QString::fromStdString(std::string(gErr->message));
        }
        g_object_unref(repoPath);

        Q_ASSERT(nullptr != repo);
        return repo;
    }

    QString getObjectPath(const QString &objName)
    {
        return QDir::cleanPath(QStringList{ ostreePath,
                                            "objects",
                                            objName.left(2),
                                            objName.right(objName.length() - 2) }
                                       .join(QDir::separator()));
    }

    // FIXME: return {Error, QStringList}
    QStringList traverseCommit(const QString &rev, int maxDepth)
    {
        GHashTable *hashTable = nullptr;
        GError *gErr = nullptr;
        QStringList objects;

        std::string str = rev.toStdString();

        if (!ostree_repo_traverse_commit(repoPtr,
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

        GVariant *object;
        for (; g_hash_table_iter_next(&iter, reinterpret_cast<gpointer *>(&object), nullptr);) {
            char *checksum;
            OstreeObjectType objectType;

            // TODO: check error
            g_variant_get(object, "(su)", &checksum, &objectType);
            QString objectName(ostree_object_to_string(checksum, objectType));
            objects.push_back(objectName);
        }

        return objects;
    }

    std::tuple<QList<OstreeRepoObject>, util::Error> findObjectsOfCommits(const QStringList &revs)
    {
        QList<OstreeRepoObject> objects;
        for (const auto &rev : revs) {
            // FIXME: check error
            auto revObjects = traverseCommit(rev, 0);
            for (const auto &objName : revObjects) {
                auto path = getObjectPath(objName);
                objects.push_back(
                        OstreeRepoObject{ .objectName = objName, .rev = rev, .path = path });
            }
        }
        return { objects, Success() };
    }

    std::tuple<QString, util::Error> resolveRev(const QString &ref)
    {
        GError *gErr = nullptr;
        char *commitID = nullptr;
        std::string refStr = ref.toStdString();
        // FIXME: should free commitID?
        if (!ostree_repo_resolve_rev(repoPtr, refStr.c_str(), false, &commitID, &gErr)) {
            return { "",
                     WrapError(NewError(gErr->code, gErr->message),
                               "ostree_repo_resolve_rev failed: " + ref) };
        }
        return { QString::fromLatin1(commitID), Success() };
    }

    // TODO: support multi refs?
    util::Error pull(const QString &ref)
    {
        GError *gErr = nullptr;
        // OstreeAsyncProgress *progress;
        // GCancellable *cancellable;
        auto repoNameStr = remoteRepoName.toStdString();
        auto refStr = ref.toStdString();
        auto refsSize = 1;
        const char *refs[refsSize + 1];
        for (decltype(refsSize) i = 0; i < refsSize; i++) {
            refs[i] = refStr.c_str();
        }
        refs[refsSize] = nullptr;

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

        // g_variant_builder_add(&builder, "{s@v}",
        // "override-url",g_variant_new_string(remote_name_or_baseurl));

        g_variant_builder_add(&builder,
                              "{s@v}",
                              "gpg-verify",
                              g_variant_new_variant(g_variant_new_boolean(false)));
        g_variant_builder_add(&builder,
                              "{s@v}",
                              "gpg-verify-summary",
                              g_variant_new_variant(g_variant_new_boolean(false)));

        auto flags = OSTREE_REPO_PULL_FLAGS_NONE;
        g_variant_builder_add(&builder,
                              "{s@v}",
                              "flags",
                              g_variant_new_variant(g_variant_new_int32(flags)));

        g_variant_builder_add(
                &builder,
                "{s@v}",
                "refs",
                g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));

        auto options = g_variant_ref_sink(g_variant_builder_end(&builder));

        if (!ostree_repo_pull_with_options(repoPtr,
                                           repoNameStr.c_str(),
                                           options,
                                           nullptr,
                                           nullptr,
                                           &gErr)) {
            qCritical() << "ostree_repo_pull_with_options failed"
                        << QString::fromStdString(std::string(gErr->message));
        }
        return Success();
    }

    QSharedPointer<InfoResponse> getRepoInfo(const QString &repoName)
    {
        QUrl url(QString("%1/%2/%3").arg(remoteEndpoint, "api/v1/repos", repoName));

        QNetworkRequest request(url);

        auto reply = httpClient.get(request);
        auto data = reply->readAll();
        qDebug() << "url" << url << "repo info" << data;
        {
            auto [info, err] = util::fromJSON<QSharedPointer<InfoResponse>>(data);
            return info;
        }
    }

    std::tuple<QString, util::Error> newUploadTask(QSharedPointer<UploadRequest> req)
    {
        QUrl url(QString("%1/api/v1/upload-tasks").arg(remoteEndpoint));
        QNetworkRequest request(url);
        qDebug() << "upload url" << url;
        auto data = std::get<0>(util::toJSON(req));

        request.setRawHeader(QByteArray("X-Token"), remoteToken.toLocal8Bit());
        auto reply = httpClient.post(request, data);
        data = reply->readAll();

        auto info(util::loadJsonBytes<UploadTaskResponse>(data));
        qDebug() << "new upload task" << data;
        if (info->code != 200) {
            return { QString(), NewError(-1, info->msg) };
        }

        return { info->data->id, Success() };
    }

    std::tuple<QString, util::Error> newUploadTask(const QString &repoName,
                                                   QSharedPointer<UploadTaskRequest> req)
    {
        QUrl url(QString("%1/api/v1/blob/%2/upload").arg(remoteEndpoint, repoName));
        QNetworkRequest request(url);

        auto data = std::get<0>(util::toJSON(req));

        request.setRawHeader(QByteArray("X-Token"), remoteToken.toLocal8Bit());
        auto reply = httpClient.post(request, data);
        data = reply->readAll();

        QSharedPointer<UploadTaskResponse> info(util::loadJsonBytes<UploadTaskResponse>(data));
        qDebug() << "new upload task" << data;
        if (info->code != 200) {
            return { QString(), NewError(-1, info->msg) };
        }

        return { info->data->id, Success() };
    }

    util::Error doUploadTask(const QString &taskID, const QString &filePath)
    {
        util::Error err;
        QByteArray fileData;

        QUrl uploadUrl(QString("%1/api/v1/upload-tasks/%2/tar").arg(remoteEndpoint, taskID));
        QNetworkRequest request(uploadUrl);
        request.setRawHeader(QByteArray("X-Token"), remoteToken.toLocal8Bit());

        QScopedPointer<QHttpMultiPart> multiPart(new QHttpMultiPart(QHttpMultiPart::FormDataType));

        // FIXME: add link support
        QList<QHttpPart> partList;
        partList.reserve(filePath.size());

        partList.push_back(QHttpPart());
        auto filePart = partList.last();

        auto *file = new QFile(filePath, multiPart.data());
        file->open(QIODevice::ReadOnly);
        filePart.setHeader(
                QNetworkRequest::ContentDispositionHeader,
                QVariant(QString(R"(form-data; name="%1"; filename="%2")").arg("file", filePath)));
        filePart.setBodyDevice(file);

        multiPart->append(filePart);
        qDebug() << "send " << filePath;

        auto reply = httpClient.put(request, multiPart.data());
        auto data = reply->readAll();

        qDebug() << "doUpload" << data;

        QSharedPointer<UploadTaskResponse> info(util::loadJsonBytes<UploadTaskResponse>(data));

        return Success();
    }

    util::Error doUploadTask(const QString &repoName,
                             const QString &taskID,
                             const QList<OstreeRepoObject> &objects)
    {
        util::Error err;
        QByteArray fileData;

        QUrl url(QString("%1/api/v1/blob/%2/upload/%3").arg(remoteEndpoint, repoName, taskID));
        QNetworkRequest request(url);
        request.setRawHeader(QByteArray("X-Token"), remoteToken.toLocal8Bit());

        QScopedPointer<QHttpMultiPart> multiPart(new QHttpMultiPart(QHttpMultiPart::FormDataType));

        // FIXME: add link support
        QList<QHttpPart> partList;
        partList.reserve(objects.size());

        for (const auto &obj : objects) {
            partList.push_back(QHttpPart());
            auto filePart = partList.last();

            QFileInfo fi(obj.path);
            if (fi.isSymLink() && false) {
                filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                   QVariant(QString("form-data; name=\"%1\"; filename=\"%2\"")
                                                    .arg("link", obj.objectName)));
                filePart.setBody(QString("%1:%2").arg(obj.objectName, obj.path).toUtf8());
            } else {
                QString objectName = obj.objectName;
                if (fi.fileName().endsWith(".file")) {
                    objectName += "z";
                    std::tie(err, fileData) = compressFile(obj.path);
                    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                       QVariant(QString(R"(form-data; name="%1"; filename="%2")")
                                                        .arg("file", objectName)));
                    filePart.setBody(fileData);
                } else {
                    auto *file = new QFile(obj.path, multiPart.data());
                    file->open(QIODevice::ReadOnly);
                    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                       QVariant(QString(R"(form-data; name="%1"; filename="%2")")
                                                        .arg("file", obj.objectName)));
                    filePart.setBodyDevice(file);
                }
            }
            multiPart->append(filePart);
            qDebug() << fi.isSymLink() << "send " << obj.objectName << obj.path;
        }

        auto reply = httpClient.put(request, multiPart.data());
        auto data = reply->readAll();

        qDebug() << "doUpload" << data;

        QSharedPointer<UploadTaskResponse> info(util::loadJsonBytes<UploadTaskResponse>(data));
        if (200 != info->code) {
            return NewError(-1, info->msg);
        }

        return Success();
    }

    util::Error cleanUploadTask(const QString &repoName, const QString &taskID)
    {
        QUrl url(QString("%1/api/v1/blob/%2/upload/%3").arg(remoteEndpoint, repoName, taskID));
        QNetworkRequest request(url);
        // FIXME: check error
        httpClient.del(request);
        return Success();
    }

    util::Error cleanUploadTask(const package::Ref &ref, const QString &filePath)
    {
        const auto savePath = QStringList{ util::getUserFile(".linglong/builder"), ref.appId }.join(
                QDir::separator());

        util::removeDir(savePath);

        QFile file(filePath);
        file.remove();

        qDebug() << "clean Upload Task";
        return Success();
    }

    util::Error getUploadStatus(const QString &taskID)
    {
        QUrl url(QString("%1/%2/%3/status").arg(remoteEndpoint, "api/v1/upload-tasks", taskID));
        QNetworkRequest request(url);

        while (1) {
            auto reply = httpClient.get(request);
            auto data = reply->readAll();

            qDebug() << "url" << url << "repo info" << data;

            auto info = util::loadJsonBytes<UploadTaskResponse>(data);

            if (200 != info->code) {
                return NewError(-1, "get upload status faild, remote server is unreachable");
            }

            if ("complete" == info->data->status) {
                break;
            } else if ("failed" == info->data->status) {
                return NewError(-1, info->data->status);
            }

            qInfo().noquote() << info->data->status;
            QThread::sleep(1);
        }

        return Success();
    }

    util::Error getToken()
    {
        QUrl url(QString("%1/%2").arg(remoteEndpoint, "api/v1/sign-in"));
        QNetworkRequest request(url);
        auto userInfo = util::getUserInfo();
        QString userJsonData = QString("{\"username\": \"%1\", \"password\": \"%2\"}")
                                       .arg(userInfo.first())
                                       .arg(userInfo.last());

        auto reply = httpClient.post(request, userJsonData.toLocal8Bit());
        auto data = reply->readAll();
        auto result = util::loadJsonBytes<AuthResponse>(data);

        qDebug() << "get token reply" << data;
        // Fixme: use status macro
        if (200 != result->code) {
            auto err = result->msg.isEmpty() ? QString("%1 is unreachable").arg(url.toString())
                                             : result->msg;
            return NewError(-1, err);
        }

        remoteToken = result->data->token;

        return Success();
    }

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

linglong::util::Error OSTreeRepo::importDirectory(const package::Ref &ref, const QString &path)
{
    Q_D(OSTreeRepo);

    auto ret = d->ostreeRun(
            { "commit", "-b", ref.toString(), "--canonical-permissions", "--tree=dir=" + path });

    return ret;
}

linglong::util::Error OSTreeRepo::renameBranch(const package::Ref &oldRef,
                                               const package::Ref &newRef)
{
    Q_D(OSTreeRepo);

    qInfo() << newRef.toOSTreeRefLocalString() << oldRef.toLocalFullRef();
    auto ret = d->ostreeRun({ "commit",
                              "-b",
                              newRef.toOSTreeRefLocalString(),
                              "--canonical-permissions",
                              "--tree=ref=" + oldRef.toLocalFullRef() });
    qInfo() << ret;
    return ret;
}

linglong::util::Error OSTreeRepo::import(const package::Bundle & /*bundle*/)
{
    return NewError(-1, "Not Implemented");
}

linglong::util::Error OSTreeRepo::exportBundle(package::Bundle & /*bundle*/)
{
    return NewError(-1, "Not Implemented");
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::list(const QString & /*filter*/)
{
    return { NewError(-1, "Not Implemented"), {} };
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::query(const QString & /*filter*/)
{
    return { NewError(-1, "Not Implemented"), {} };
}

linglong::util::Error OSTreeRepo::push(const package::Ref &ref)
{
    Q_D(OSTreeRepo);

    auto ret = d->getToken();
    if (ret) {
        return WrapError(ret, "get token failed");
    }

    QSharedPointer<UploadRequest> uploadReq(new UploadRequest);

    uploadReq->repoName = d->remoteRepoName;
    uploadReq->ref = ref.toOSTreeRefLocalString();

    // FIXME: no need,use /v1/meta/:id
    QSharedPointer<InfoResponse> repoInfo(d->getRepoInfo(d->remoteRepoName));
    if (repoInfo->code != 200) {
        return NewError(-1, repoInfo->msg);
    }

    // send files
    QString taskID;
    {
        auto [result, err] = d->newUploadTask(uploadReq);
        if (err) {
            return WrapError(err, "call newUploadTask failed");
        }
        taskID = std::move(result);
    }

    // compress form data
    QString filePath;
    {
        auto [result, err] = compressOstreeData(ref);
        if (err) {
            return WrapError(err, "compress ostree data failed");
        }
        filePath = std::move(result);
    }

    {
        auto err = d->doUploadTask(taskID, filePath);
        if (err) {
            return WrapError(err, "call doUploadTask failed");
        }
    }

    {
        auto err = d->getUploadStatus(taskID);
        if (err) {
            d->cleanUploadTask(ref, filePath);
            return err;
        }
    }

    return WrapError(d->cleanUploadTask(ref, filePath), "call cleanUploadTask failed");
}

linglong::util::Error OSTreeRepo::push(const package::Ref &ref, bool /*force*/)
{
    Q_D(OSTreeRepo);

    {
        auto err = d->getToken();
        if (err) {
            return WrapError(err, "get token failed");
        }
    }

    QSharedPointer<UploadTaskRequest> uploadTaskReq(new UploadTaskRequest);

    // FIXME: no need,use /v1/meta/:id
    QSharedPointer<InfoResponse> repoInfo(d->getRepoInfo(d->remoteRepoName));
    if (repoInfo->code != 200) {
        return NewError(-1, repoInfo->msg);
    }

    QString commitID;
    {
        auto [result, err] = d->resolveRev(ref.toOSTreeRefLocalString());
        if (err) {
            return WrapError(err, "push failed:" + ref.toOSTreeRefLocalString());
        }
        commitID = result;
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
        auto [result, err] = d->findObjectsOfCommits({ commitID });
        if (err) {
            return WrapError(err, "call findObjectsOfCommits failed");
        }
        objects = result;
    }

    for (auto const &obj : objects) {
        uploadTaskReq->objects.push_back(obj.objectName);
    }

    // send files
    QString taskID;
    {
        auto [result, err] = d->newUploadTask(d->remoteRepoName, uploadTaskReq);
        if (err) {
            return WrapError(err, "call newUploadTask failed");
        }
        taskID = result;
    }

    {
        auto err = d->doUploadTask(d->remoteRepoName, taskID, objects);
        if (err) {
            d->cleanUploadTask(d->remoteRepoName, taskID);
            return WrapError(err, "call newUploadTask failed");
        }
    }

    return WrapError(d->cleanUploadTask(d->remoteRepoName, taskID), "call cleanUploadTask failed");
}

linglong::util::Error OSTreeRepo::push(const package::Bundle & /*bundle*/, bool /*force*/)
{
    return NewError(-1, "Not Implemented");
}

linglong::util::Error OSTreeRepo::pull(const package::Ref &ref, bool /*force*/)
{
    // FIXME(black_desk): should implement force
    Q_D(OSTreeRepo);

    // FIXME(black_desk): When a error raised from libcurl, libostree will treat
    // it like a fail, but not a temporary error, which make the default retry
    // (5 times) useless. So we now have to retry some times to overcome this
    // problem.
    // As we have try the current base will fail so many times during
    // transferring. So we decide to retry 30 times.
    int retry = 30;
    util::Error err;
    while (retry--) {
        qDebug() << "remaining retries" << retry;
        err = WrapError(d->ostreeRun({ "pull", ref.toString() }), "");
        if (err) {
            break;
        }
    }
    return err;
}

linglong::util::Error OSTreeRepo::pullAll(const package::Ref &ref, bool /*force*/)
{
    // Q_D(OSTreeRepo);

    // FIXME(black-desk): pullAll should not belong to this class.

    auto err = pull(package::Ref(QStringList{ ref.toString(), "runtime" }.join("/")), false);
    if (err) {
        return err;
    }

    // FIXME: some old package have no devel, ignore error for now.
    auto _ = pull(package::Ref(QStringList{ ref.toString(), "devel" }.join("/")), false);

    return Success();
}

linglong::util::Error OSTreeRepo::init(const QString &mode)
{
    Q_D(OSTreeRepo);

    return d->ostreeRun({ "init", QString("--mode=%1").arg(mode) });
}

linglong::util::Error OSTreeRepo::remoteAdd(const QString &repoName, const QString &repoUrl)
{
    Q_D(OSTreeRepo);

    return d->ostreeRun({ "remote", "add", "--no-gpg-verify", repoName, repoUrl });
}

linglong::util::Error OSTreeRepo::remoteDelete(const QString &repoName)
{
    Q_D(OSTreeRepo);

    return d->ostreeRun({ "remote", "delete", repoName });
}

OSTreeRepo::OSTreeRepo(const QString &path)
    : dd_ptr(new OSTreeRepoPrivate(path, "", "", this))
{
}

OSTreeRepo::OSTreeRepo(const QString &localRepoPath,
                       const QString &remoteEndpoint,
                       const QString &remoteRepoName)
    : dd_ptr(new OSTreeRepoPrivate(localRepoPath, remoteEndpoint, remoteRepoName, this))
{
}

linglong::util::Error OSTreeRepo::checkout(const package::Ref &ref,
                                           const QString &subPath,
                                           const QString &target)
{
    QStringList args = { "checkout", "--union", "--force-copy" };
    if (!subPath.isEmpty()) {
        args.push_back("--subpath=" + subPath);
    }
    args.append({ ref.toString(), target });
    return dd_ptr->ostreeRun(args);
}

linglong::util::Error OSTreeRepo::checkoutAll(const package::Ref &ref,
                                              const QString &subPath,
                                              const QString &target)
{
    Q_D(OSTreeRepo);

    QStringList runtimeArgs = { "checkout",
                                "--union",
                                QStringList{ ref.toString(), "runtime" }.join("/"),
                                target };
    QStringList develArgs = { "checkout",
                              "--union",
                              QStringList{ ref.toString(), "devel" }.join("/"),
                              target };

    if (!subPath.isEmpty()) {
        runtimeArgs.push_back("--subpath=" + subPath);
        develArgs.push_back("--subpath=" + subPath);
    }

    auto err = d->ostreeRun(runtimeArgs);

    if (err) {
        return WrapError(err, "");
    }

    err = d->ostreeRun(develArgs);

    // Fixme: some old package have no devel, ignore error for now.
    return Success();
}

QString OSTreeRepo::rootOfLayer(const package::Ref &ref)
{
    return QStringList{ dd_ptr->repoRootPath, "layers", ref.appId, ref.version, ref.arch }.join(
            QDir::separator());
}

bool OSTreeRepo::isRefExists(const package::Ref &ref)
{
    Q_D(OSTreeRepo);
    auto runtimeRef = ref.toString() + '/' + "runtime";
    auto err = util::Exec(
            "sh",
            { "-c",
              QString("ostree refs --repo=%1 | grep -Fx %2").arg(d->ostreePath).arg(runtimeRef) });
    return !static_cast<bool>(err);
}

QString OSTreeRepo::remoteShowUrl(const QString &repoName)
{
    Q_D(OSTreeRepo);

    QByteArray ostreeOutput;
    QString repoUrl;
    auto ret = d->ostreeRun({ "remote", "show-url", repoName }, &ostreeOutput);

    for (const auto &item : QString::fromLocal8Bit(ostreeOutput).trimmed().split('\n')) {
        if (!item.trimmed().isEmpty()) {
            repoUrl = item;
        }
    }

    return repoUrl;
}

package::Ref OSTreeRepo::localLatestRef(const package::Ref &ref)
{
    Q_D(OSTreeRepo);

    QString latestVer = "latest";

    QString args = QString("ostree refs --repo=%1 | grep %2 | grep %3")
                           .arg(d->ostreePath, ref.appId, util::hostArch() + "/" + "runtime");

    QSharedPointer<QByteArray> output;
    auto err = util::Exec("sh", { "-c", args }, -1, output);

    if (!err) {
        auto outputText = QString::fromLocal8Bit(*output);
        auto lines = outputText.split('\n');
        // last line of result is null, remove it
        lines.removeLast();
        latestVer = linglong::util::latestVersion(lines);
    }

    return package::Ref("", ref.channel, ref.appId, latestVer, ref.arch, ref.module);
}

package::Ref OSTreeRepo::remoteLatestRef(const package::Ref &ref)
{
    Q_D(OSTreeRepo);

    QString latestVer = "unknown";
    QString ret;

    if (!HTTPCLIENT->queryRemoteApp(d->remoteRepoName,
                                    d->remoteEndpoint,
                                    ref.appId,
                                    ref.version,
                                    util::hostArch(),
                                    ret)) {
        qCritical() << "query remote app failed";
        return package::Ref(ref.repo, ref.channel, ref.appId, latestVer, ref.arch, ref.module);
    }

    // FIXME(black_desk): handle error
    auto infoList = std::get<0>(
            util::fromJSON<QList<QSharedPointer<linglong::package::Info>>>(ret.toUtf8()));

    for (auto info : infoList) {
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
        auto localRepoRoot = QString(dd_ptr->repoRootPath) + "/layers" + "/" + appId;

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
    QStringList args = { "refs", "--delete", ref.toString() };
    auto err = dd_ptr->ostreeRun(args);
    if (err) {
        return WrapError(err, "delete refs failed");
    }

    args = QStringList{ "prune" };
    return WrapError(dd_ptr->ostreeRun(args), "");
}

std::tuple<linglong::util::Error, QStringList> OSTreeRepo::remoteList()
{
    QStringList remoteList;
    QStringList args = { "remote", "list" };
    QByteArray output;
    auto err = dd_ptr->ostreeRun(args, &output);
    if (err) {
        return { WrapError(err, "remote list failed"), QStringList{} };
    }

    for (const auto &item : QString::fromLocal8Bit(output).trimmed().split('\n')) {
        if (!item.trimmed().isEmpty()) {
            remoteList.push_back(item);
        }
    }

    return { Success(), remoteList };
}

std::tuple<QString, util::Error> OSTreeRepo::compressOstreeData(const package::Ref &ref)
{
    // check out ostree data
    // Fixme: use /tmp
    const auto savePath = QStringList{ util::getUserFile(".linglong/builder"), ref.appId }.join(
            QDir::separator());
    util::ensureDir(savePath);

    auto err =
            checkout(package::Ref("", ref.appId, ref.version, ref.arch, ref.module), "", savePath);
    if (err) {
        return { QString(),
                 NewError(-1, QString("checkout %1 to %2 failed").arg(ref.appId).arg(savePath)) };
    }

    // compress data
    QStringList args;
    const QString fileName = QString("%1.tgz").arg(ref.appId);
    const QString filePath =
            QStringList{ util::getUserFile(".linglong/builder"), fileName }.join(QDir::separator());

    QDir::setCurrent(savePath);

    args << "-zcf" << filePath << ".";

    // TODO: handle error of tar
    err = util::Exec("tar", args);
    qDebug() << "tar with exit code:" << err.code() << err.message();

    return { filePath, Success() };
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace repo
} // namespace linglong
