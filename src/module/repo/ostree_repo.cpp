/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include "ostree_repo.h"

#include <QProcess>
#include <QDir>
#include <QHttpPart>
#include <utility>

#include "module/package/ref.h"
#include "module/package/info.h"
#include "module/util/runner.h"
#include "module/util/sysinfo.h"
#include "module/util/version.h"
#include "module/util/http/http_client.h"
#include "module/util/config/config.h"
#include "module/util/semver.h"
#include "module/util/httpclient.h"

namespace linglong {
namespace repo {

struct OstreeRepoObject {
    QString objectName;
    QString rev;
    QString path;
};

typedef QMap<QString, OstreeRepoObject> RepoObjectMap;

class OSTreeRepoPrivate
{
    OSTreeRepoPrivate(QString localRepoRootPath, QString remoteEndpoint, QString remoteRepoName, OSTreeRepo *parent)
        : repoRootPath(std::move(localRepoRootPath))
        , remoteEndpoint(std::move(remoteEndpoint))
        , remoteRepoName(std::move(remoteRepoName))
        , q_ptr(parent)
    {
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

    static QByteArray glibBytesToQByteArray(GBytes *bytes)
    {
        const void *data;
        gsize length;
        data = g_bytes_get_data(bytes, &length);
        return {reinterpret_cast<const char *>(data), static_cast<int>(length)};
    }

    static GBytes *glibInputStreamToBytes(GInputStream *inputStream)
    {
        g_autoptr(GOutputStream) outStream = nullptr;
        g_autoptr(GError) gErr = nullptr;

        if (inputStream == nullptr)
            return g_bytes_new(nullptr, 0);

        outStream = g_memory_output_stream_new(nullptr, 0, g_realloc, g_free);
        g_output_stream_splice(outStream, inputStream, G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, nullptr, &gErr);
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
            return {NewError(errno, "open file failed: " + filepath), QByteArray()};
        }

        info =
            g_file_query_info(file, G_FILE_ATTRIBUTE_UNIX_MODE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
        guint32 mode = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE);

        info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 nullptr, nullptr);
        if (g_file_info_get_is_symlink(info)) {
            info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
            g_file_info_set_file_type(info, G_FILE_TYPE_SYMBOLIC_LINK);
        } else {
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

        return {NoError(), glibBytesToQByteArray(zlibBytes)};
    }

    static OstreeRepo *openRepo(const QString &path)
    {
        GError *gErr = nullptr;
        std::string repoPathStr = path.toStdString();

        auto repoPath = g_file_new_for_path(repoPathStr.c_str());
        auto repo = ostree_repo_new(repoPath);
        if (!ostree_repo_open(repo, nullptr, &gErr)) {
            qCritical() << "open repo" << path << "failed" << QString::fromStdString(std::string(gErr->message));

            repo = nullptr;
        }

        g_object_unref(repoPath);

        return repo;
    }

    QString getObjectPath(const QString &objName)
    {
        return QDir::cleanPath(
            QStringList {ostreePath, "objects", objName.left(2), objName.right(objName.length() - 2)}.join(
                QDir::separator()));
    }

    // FIXME: return {Error, QStringList}
    QStringList traverseCommit(const QString &rev, int maxDepth)
    {
        GHashTable *hashTable = nullptr;
        GError *gErr = nullptr;
        QStringList objects;

        std::string str = rev.toStdString();
        if (!repoPtr) {
            qCritical() << "null repo";
            return {};
        }

        if (!ostree_repo_traverse_commit(repoPtr, str.c_str(), maxDepth, &hashTable, nullptr, &gErr)) {
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
                objects.push_back(OstreeRepoObject {.objectName = objName, .rev = rev, .path = path});
            }
        }
        return {objects, NoError()};
    }

    std::tuple<QString, util::Error> resolveRev(const QString &ref)
    {
        GError *gErr = nullptr;
        char *commitID = nullptr;
        std::string refStr = ref.toStdString();
        // FIXME: should free commitID?
        if (!ostree_repo_resolve_rev(repoPtr, refStr.c_str(), false, &commitID, &gErr)) {
            return {"", WrapError(NewError(gErr->code, gErr->message), "ostree_repo_resolve_rev failed: " + ref)};
        }
        return {QString::fromLatin1(commitID), NoError()};
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

        // g_variant_builder_add(&builder, "{s@v}", "override-url",g_variant_new_string(remote_name_or_baseurl));

        g_variant_builder_add(&builder, "{s@v}", "gpg-verify", g_variant_new_variant(g_variant_new_boolean(false)));
        g_variant_builder_add(&builder, "{s@v}", "gpg-verify-summary",
                              g_variant_new_variant(g_variant_new_boolean(false)));

        auto flags = OSTREE_REPO_PULL_FLAGS_NONE;
        g_variant_builder_add(&builder, "{s@v}", "flags", g_variant_new_variant(g_variant_new_int32(flags)));

        g_variant_builder_add(&builder, "{s@v}", "refs",
                              g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));

        auto options = g_variant_ref_sink(g_variant_builder_end(&builder));

        if (!ostree_repo_pull_with_options(repoPtr, repoNameStr.c_str(), options, nullptr, nullptr, &gErr)) {
            qCritical() << "ostree_repo_pull_with_options failed" << QString::fromStdString(std::string(gErr->message));
        }
        return NoError();
    }

    InfoResponse *getRepoInfo(const QString &repoName)
    {
        QUrl url(QString("%1/%2/%3").arg(ConfigInstance().repoUrl, "v1/repo", repoName));

        QNetworkRequest request(url);

        auto reply = httpClient.get(request);
        auto data = reply->readAll();
        auto info = util::loadJsonBytes<InfoResponse>(data);
        return info;
    }

    std::tuple<QString, util::Error> newUploadTask(const QString &repoName, UploadTaskRequest *req)
    {
        QUrl url(QString("%1/v1/blob/%2/upload").arg(ConfigInstance().repoUrl, repoName));
        QNetworkRequest request(url);

        auto data = QJsonDocument(toVariant(req).toJsonObject()).toJson();

        auto reply = httpClient.post(request, data);
        data = reply->readAll();

        QScopedPointer<UploadTaskResponse> info(util::loadJsonBytes<UploadTaskResponse>(data));
        return {info->id, NoError()};
    }

    util::Error doUploadTask(const QString &repoName, const QString &taskID, const QList<OstreeRepoObject> &objects)
    {
        util::Error err(NoError());
        QByteArray fileData;

        QUrl url(QString("%1/v1/blob/%2/upload/%3").arg(ConfigInstance().repoUrl, repoName, taskID));
        QNetworkRequest request(url);

        QScopedPointer<QHttpMultiPart> multiPart(new QHttpMultiPart(QHttpMultiPart::FormDataType));

        // FIXME: add link support
        QList<QHttpPart> partList;
        partList.reserve(objects.size());

        for (const auto &obj : objects) {
            partList.push_back(QHttpPart());
            auto filePart = partList.last();

            QFileInfo fi(obj.path);
            if (fi.isSymLink() && false) {
                filePart.setHeader(
                    QNetworkRequest::ContentDispositionHeader,
                    QVariant(QString("form-data; name=\"%1\"; filename=\"%2\"").arg("link", obj.objectName)));
                filePart.setBody(QString("%1:%2").arg(obj.objectName, obj.path).toUtf8());
            } else {
                QString objectName = obj.objectName;
                if (fi.fileName().endsWith(".file")) {
                    objectName += "z";
                    std::tie(err, fileData) = compressFile(obj.path);
                    filePart.setHeader(
                        QNetworkRequest::ContentDispositionHeader,
                        QVariant(QString(R"(form-data; name="%1"; filename="%2")").arg("file", objectName)));
                    filePart.setBody(fileData);
                } else {
                    auto *file = new QFile(obj.path, multiPart.data());
                    file->open(QIODevice::ReadOnly);
                    filePart.setHeader(
                        QNetworkRequest::ContentDispositionHeader,
                        QVariant(QString(R"(form-data; name="%1"; filename="%2")").arg("file", obj.objectName)));
                    filePart.setBodyDevice(file);
                }
            }
            multiPart->append(filePart);
            qDebug() << fi.isSymLink() << "send " << obj.objectName << obj.path;
        }

        // FIXME: check error
        httpClient.put(request, multiPart.data());

        return NoError();
    }

    util::Error cleanUploadTask(const QString &repoName, const QString &taskID)
    {
        QUrl url(QString("%1/v1/blob/%2/upload/%3").arg(ConfigInstance().repoUrl, repoName, taskID));
        QNetworkRequest request(url);
        // FIXME: check error
        httpClient.del(request);
        return NoError();
    }

    QString repoRootPath;
    QString remoteEndpoint;
    QString remoteRepoName;

    OstreeRepo *repoPtr = nullptr;
    QString ostreePath;

    util::HttpRestClient httpClient;

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
    return NoError();
}

linglong::util::Error OSTreeRepo::exportBundle(package::Bundle &bundle)
{
    return NoError();
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::list(const QString &filter)
{
    return {NoError(), {}};
}

std::tuple<linglong::util::Error, QList<package::Ref>> OSTreeRepo::query(const QString &filter)
{
    return {NoError(), {}};
}

linglong::util::Error OSTreeRepo::push(const package::Ref &ref, bool force)
{
    Q_D(OSTreeRepo);

    util::Error err(NoError());
    QList<OstreeRepoObject> objects;
    UploadTaskRequest uploadTaskReq;

    // FIXME: no need,use /v1/meta/:id
    QScopedPointer<InfoResponse> repoInfo(d->getRepoInfo(d->remoteRepoName));

    QString commitID;
    std::tie(commitID, err) = d->resolveRev(ref.toLocalFullRef());
    if (!err.success()) {
        return WrapError(err, "push failed:" + ref.toLocalFullRef());
    }
    qDebug() << "push commit" << commitID << ref.toLocalFullRef();

    auto revPair = new RevPair(&uploadTaskReq);
    uploadTaskReq.refs[ref.toLocalFullRef()] = revPair;
    revPair->client = commitID;
    // FIXME: get server version to compare
    revPair->server = "";

    // find files to commit
    std::tie(objects, err) = d->findObjectsOfCommits({commitID});
    if (!err.success()) {
        return WrapError(err, "call findObjectsOfCommits failed");
    }

    for (auto const &obj : objects) {
        uploadTaskReq.objects.push_back(obj.objectName);
    }

    // send files
    QString taskID;
    std::tie(taskID, err) = d->newUploadTask(d->remoteRepoName, &uploadTaskReq);
    if (!err.success()) {
        return WrapError(err, "call newUploadTask failed");
    }

    d->doUploadTask(d->remoteRepoName, taskID, objects);
    if (!err.success()) {
        d->cleanUploadTask(d->remoteRepoName, taskID);
        return WrapError(err, "call newUploadTask failed");
    }

    return WrapError(d->cleanUploadTask(d->remoteRepoName, taskID), "call cleanUploadTask failed");
}

linglong::util::Error OSTreeRepo::push(const package::Bundle &bundle, bool force)
{
    return NoError();
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
    : dd_ptr(new OSTreeRepoPrivate(path, "", "", this))
{
}

OSTreeRepo::OSTreeRepo(const QString &localRepoPath, const QString &remoteEndpoint, const QString &remoteRepoName)
    : dd_ptr(new OSTreeRepoPrivate(localRepoPath, remoteEndpoint, remoteRepoName, this))
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
        return ret;
    }

    ret = d->ostreeRun(develArgs);

    return WrapError(ret);
}

QString OSTreeRepo::rootOfLayer(const package::Ref &ref)
{
    return QStringList {dd_ptr->repoRootPath, "layers", ref.appId, ref.version, ref.arch}.join(QDir::separator());
}

bool OSTreeRepo::isRefExists(const package::Ref &ref)
{
    Q_D(OSTreeRepo);
    auto runtimeRef = ref.toString() + '/' + "runtime";
    auto ret = runner::Runner(
        "sh", {"-c", QString("ostree refs --repo=%1 | grep -Fx %2").arg(d->ostreePath).arg(runtimeRef)}, -1);

    return ret;
}

package::Ref OSTreeRepo::localLatestRef(const package::Ref &ref)
{
    Q_D(OSTreeRepo);

    QString latestVer = "latest";

    QString args = QString("ostree refs repo --repo=%1 | grep %2 | grep %3")
                       .arg(d->ostreePath)
                       .arg(ref.appId)
                       .arg(util::hostArch() + "/" + "runtime");

    auto result = runner::RunnerRet("sh", {"-c", args}, -1);

    if (std::get<0>(result)) {
        // last line of result is null, remove it
        std::get<1>(result).removeLast();

        latestVer = linglong::util::latestVersion(std::get<1>(result));
    }

    return package::Ref("", ref.channel, ref.appId, latestVer, ref.arch, ref.module);
}

package::Ref OSTreeRepo::remoteLatestRef(const package::Ref &ref)
{
    Q_D(OSTreeRepo);

    QString latestVer = "latest";
    QString ret;

    if (!G_HTTPCLIENT->queryRemoteApp(ref.appId, "", util::hostArch(), ret)) {
        qCritical() << "query remote app failed";
        return package::Ref("", ref.channel, ref.appId, latestVer, ref.arch, ref.module);
    }

    auto retObject = QJsonDocument::fromJson(ret.toUtf8()).object();

    auto infoList = fromVariantList<linglong::package::InfoList>(retObject.value(QStringLiteral("data")));

    for (auto info : infoList) {
        latestVer = linglong::util::compareVersion(latestVer, info->version) > 0 ? latestVer : info->version;
    }

    return package::Ref("", ref.channel, ref.appId, latestVer, ref.arch, ref.module);
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
