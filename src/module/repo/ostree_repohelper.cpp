/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ostree_repohelper.h"
#include "module/util/file.h"
#include "module/util/runner.h"
#include "module/util/version/version.h"

const int MAX_ERRINFO_BUFSIZE = 512;

namespace linglong {

/*
 * 保存本地仓库信息
 *
 * @param basedir: 临时仓库路径
 * @param repo: 错误信息
 *
 */
void OstreeRepoHelper::setDirInfo(const QString &basedir, OstreeRepo *repo)
{
    pLingLongDir->basedir = basedir;
    pLingLongDir->repo = repo;
}

OstreeRepoHelper::OstreeRepoHelper()
{
    pLingLongDir = new LingLongDir();
    pLingLongDir->repo = nullptr;
}

OstreeRepoHelper::~OstreeRepoHelper()
{
    if (pLingLongDir != nullptr) {
        qInfo() << "~OstreeRepoHelper() called";
        // free ostree repo
        if (pLingLongDir->repo) {
            g_clear_object(&pLingLongDir->repo);
        }
        delete pLingLongDir;
        pLingLongDir = nullptr;
    }
}

/*
 * 创建本地repo仓库,当目标路径不存在时，自动新建路径
 *
 * @param repoPath: 本地仓库路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool OstreeRepoHelper::ensureRepoEnv(const QString &repoPath, QString &err)
{
    const std::string repoPathTmp = repoPath.toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPathTmp.empty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }
    OstreeRepo *repo;
    g_autoptr(GFile) repodir = NULL;
    std::string tmpPath = "";
    // 适配目标路径末尾的‘/’，本地仓库目录名为repo
    if (repoPathTmp.at(repoPathTmp.size() - 1) == '/') {
        tmpPath = repoPathTmp + "repo";
    } else {
        tmpPath = repoPathTmp + "/repo";
    }
    qInfo() << "ensureRepoEnv repo path:" << QString::fromStdString(tmpPath);
    if (!pLingLongDir->repo) {
        repodir = g_file_new_for_path(tmpPath.c_str());
        repo = ostree_repo_new(repodir);
        // 校验创建的仓库是否ok
        if (!ostree_repo_open(repo, cancellable, &error)) {
            qInfo() << "ostree_repo_open error:" << error->message;
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_open:%s error:%s", __FILE__, __func__,
                     repoPathTmp.c_str(), error->message);
            err = QString(QLatin1String(info));
            g_object_unref(repodir);
            return false;
        }
        // set dir path info
        setDirInfo(repoPath, repo);
    }
    return true;
}

/*
 * 查询ostree远端仓库列表
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param vec: 远端仓库列表
 * @param err: 错误信息
 *
 * @return bool: true:查询成功 false:失败
 */
bool OstreeRepoHelper::getRemoteRepoList(const QString &repoPath, QVector<QString> &vec, QString &err)
{
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPath.isEmpty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }
    // 校验本地仓库是否创建
    if (!pLingLongDir->repo || pLingLongDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }
    g_auto(GStrv) res = NULL;
    if (pLingLongDir->repo) {
        res = ostree_repo_remote_list(pLingLongDir->repo, NULL);
    }

    if (res != NULL) {
        for (int i = 0; res[i] != NULL; i++) {
            // vec.push_back(res[i]);
            vec.append(QLatin1String(res[i]));
        }
    } else {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s no remote repo found", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    return true;
}

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
bool OstreeRepoHelper::fetchRemoteSummary(OstreeRepo *repo, const char *name, GBytes **outSummary,
                                          GBytes **outSummarySig, GCancellable *cancellable, GError **error)
{
    g_autofree char *url = NULL;
    g_autoptr(GBytes) summary = NULL;
    g_autoptr(GBytes) summarySig = NULL;

    if (!ostree_repo_remote_get_url(repo, name, &url, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_get_url error:%s\n", (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_get_url error:" << (*error)->message;
        return false;
    }
    // fprintf(stdout, "fetchRemoteSummary remote %s,url:%s\n", name, url);

    if (!ostree_repo_remote_fetch_summary(repo, name, &summary, &summarySig, cancellable, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_fetch_summary error:%s\n", (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_fetch_summary error:" << (*error)->message;
        return false;
    }

    if (summary == NULL) {
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
 * 从summary中的refMap中获取仓库所有软件包索引refs
 *
 * @param ref_map: summary信息中解析出的ref map信息
 * @param outRefs: 仓库软件包索引信息
 */
void OstreeRepoHelper::getPkgRefsFromRefsMap(GVariant *ref_map, std::map<std::string, std::string> &outRefs)
{
    GVariant *value;
    GVariantIter ref_iter;
    g_variant_iter_init(&ref_iter, ref_map);
    while ((value = g_variant_iter_next_value(&ref_iter)) != NULL) {
        /* helper for being able to auto-free the value */
        g_autoptr(GVariant) child = value;
        const char *ref_name = NULL;
        g_variant_get_child(child, 0, "&s", &ref_name);
        if (ref_name == NULL)
            continue;
        g_autofree char *ref = NULL;
        ostree_parse_refspec(ref_name, NULL, &ref, NULL);
        if (ref == NULL)
            continue;
        // gboolean is_app = g_str_has_prefix(ref, "app/");

        g_autoptr(GVariant) csum_v = NULL;
        char tmp_checksum[65];
        const guchar *csum_bytes;
        g_variant_get_child(child, 1, "(t@aya{sv})", NULL, &csum_v, NULL);
        csum_bytes = ostree_checksum_bytes_peek_validate(csum_v, NULL);
        if (csum_bytes == NULL)
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
 * 从ostree仓库描述文件Summary信息中获取仓库所有软件包索引refs
 *
 * @param summary: 远端仓库Summary信息
 * @param outRefs: 远端仓库软件包索引信息
 */
void OstreeRepoHelper::getPkgRefsBySummary(GVariant *summary, std::map<std::string, std::string> &outRefs)
{
    // g_autoptr(GHashTable) ret_all_refs = NULL;
    g_autoptr(GVariant) ref_map = NULL;
    // g_autoptr(GVariant) metadata = NULL;

    // ret_all_refs = g_hash_table_new(linglong_collection_ref_hash, linglong_collection_ref_equal);
    ref_map = g_variant_get_child_value(summary, 0);
    // metadata = g_variant_get_child_value(summary, 1);
    getPkgRefsFromRefsMap(ref_map, outRefs);
}

/*
 * 查询远端仓库所有软件包索引信息refs
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param remoteName: 远端仓库名称
 * @param outRefs: 远端仓库软件包索引信息(key:refs, value:commit)
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool OstreeRepoHelper::getRemoteRefs(const QString &repoPath, const QString &remoteName,
                                     QMap<QString, QString> &outRefs, QString &err)
{
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.isEmpty()) {
        // fprintf(stdout, "getRemoteRefs param err\n");
        qInfo() << "getRemoteRefs param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!pLingLongDir->repo || pLingLongDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }
    const std::string remoteNameTmp = remoteName.toStdString();
    g_autoptr(GBytes) summaryBytes = NULL;
    g_autoptr(GBytes) summarySigBytes = NULL;
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    bool ret = fetchRemoteSummary(pLingLongDir->repo, remoteNameTmp.c_str(), &summaryBytes, &summarySigBytes,
                                  cancellable, &error);
    if (!ret) {
        if (err != NULL) {
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s err:%s", __FILE__, __func__, error->message);
            err = info;
        } else {
            err = "getRemoteRefs remote repo err";
        }
        return false;
    }
    GVariant *summary =
        g_variant_ref_sink(g_variant_new_from_bytes(OSTREE_SUMMARY_GVARIANT_FORMAT, summaryBytes, FALSE));
    // std::map转QMap
    std::map<std::string, std::string> outRet;
    getPkgRefsBySummary(summary, outRet);
    for (auto iter = outRet.begin(); iter != outRet.end(); ++iter) {
        outRefs.insert(QString::fromStdString(iter->first), QString::fromStdString(iter->second));
    }
    return true;
}

/*
 * 按照指定字符分割字符串
 *
 * @param str: 目标字符串
 * @param separator: 分割字符串
 * @param result: 分割结果
 */
void OstreeRepoHelper::splitStr(std::string str, std::string separator, std::vector<std::string> &result)
{
    size_t cutAt;
    while ((cutAt = str.find_first_of(separator)) != str.npos) {
        if (cutAt > 0) {
            result.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if (str.length() > 0) {
        result.push_back(str);
    }
}

/*
 * 解析仓库软件包索引ref信息
 *
 * @param fullRef: 目标软件包索引ref信息
 * @param result: 解析结果
 *
 * @return bool: true:成功 false:失败
 */
bool OstreeRepoHelper::resolveRef(const std::string &fullRef, std::vector<std::string> &result)
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
 * 查询软件包在仓库中对应的索引信息ref，版本号为空查询最新版本，非空查询指定版本
 *
 * @param repoPath: 远端仓库对应的本地仓库路径
 * @param remoteName: 远端仓库名称
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包对应的版本号
 * @param arch: 软件包对应的架构名
 * @param matchRef: 软件包对应的索引信息ref
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool OstreeRepoHelper::queryMatchRefs(const QString &repoPath, const QString &remoteName, const QString &pkgName,
                                      const QString &pkgVer, const QString &arch, QString &matchRef, QString &err)
{
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.isEmpty() || pkgName.isEmpty()) {
        // fprintf(stdout, "queryMatchRefs param err\n");
        qInfo() << "queryMatchRefs param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!pLingLongDir->repo || pLingLongDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    const std::string pkgNameTmp = pkgName.toStdString();
    QMap<QString, QString> outRefs;
    bool ret = getRemoteRefs(repoPath, remoteName, outRefs, err);
    if (ret) {
        QString filterString = "(" + pkgName + "/)+";
        QList<QString> keyRefs = outRefs.keys().filter(QRegExp(filterString, Qt::CaseSensitive));
        // 版本号不为空，查找指定版本
        if (!pkgVer.isEmpty()) {
            for (QString ref : keyRefs) {
                std::vector<std::string> result;
                ret = resolveRef(ref.toStdString(), result);
                // new ref format org.deepin.calculator/1.2.2/x86_64
                if (ret && result[1] == pkgVer.toStdString() && result[2] == arch.toStdString()) {
                    matchRef = ref;
                    return true;
                }
            }
        } else {
            bool flag = false;
            QString curVersion = linglong::util::APP_MIN_VERSION;
            for (QString ref : keyRefs) {
                std::vector<std::string> result;
                ret = resolveRef(ref.toStdString(), result);
                linglong::util::AppVersion curVer(curVersion);
                // 玲珑适配
                // new ref format org.deepin.calculator/1.2.2/x86_64
                if (ret) {
                    QString dstVersion = QString::fromStdString(result[1]);
                    linglong::util::AppVersion dstVer(dstVersion);
                    if (!dstVer.isValid()) {
                        qInfo() << "queryMatchRefs ref:" << ref << " version is not valid";
                        continue;
                    }
                    if (dstVer.isBigThan(curVer)) {
                        curVersion = dstVersion;
                        matchRef = ref;
                        flag = true;
                    }
                }
            }
            if (flag) {
                return true;
            }
        }
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s %s not found in remote repo", __FILE__, __func__,
                 pkgNameTmp.c_str());
        err = info;
    }
    return false;
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
bool OstreeRepoHelper::checkOutAppData(const QString &repoPath, const QString &remoteName, const QString &ref,
                                       const QString &dstPath, QString &err)
{
    // ostree --repo=repo checkout -U --union org.deepin.calculator/x86_64/1.2.2 /persistent/linglong/layers/XXX
    linglong::util::createDir(dstPath);
    auto ret = linglong::runner::Runner(
        "ostree", {"--repo=" + repoPath + "/repo", "checkout", "-U", "--union", ref, dstPath}, 1000 * 60 * 60 * 24);
    if (!ret) {
        err = "checkOutAppData " + ref + " err";
        qCritical() << "checkOutAppData err, repoPath:" << repoPath << ", remoteName:" << remoteName
                    << ", dstPath:" << dstPath << ", ref:" << ref;
        return false;
    }
    return true;
}

/*
 * 通过父进程id查找子进程id
 *
 * @param qint64: 父进程id
 *
 * @return qint64: 子进程id
 */
qint64 OstreeRepoHelper::getChildPid(qint64 pid)
{
    QProcess process;
    const QStringList argList = {"-P", QString::number(pid)};
    process.start("pgrep", argList);
    if (!process.waitForStarted()) {
        qCritical() << "start pgrep failed!";
        return 0;
    }
    if (!process.waitForFinished(1000 * 60)) {
        qCritical() << "run pgrep finish failed!";
        return 0;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    QString childPid = "";
    if (retStatus != 0 || retCode != 0) {
        qCritical() << "run pgrep failed, retCode:" << retCode << ", args:" << argList
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
        return 0;
    } else {
        childPid = QString::fromLocal8Bit(process.readAllStandardOutput());
        qDebug() << "getChildPid ostree pid:" << childPid;
    }

    return childPid.toLongLong();
}

/*
 * 启动一个ostree 命令相关的任务
 *
 * @param cmd: 需要运行的命令
 * @param ref: ostree软件包对应的ref
 * @param argList: 参数列表
 * @param timeout: 任务超时时间
 *
 * @return bool: true:成功 false:失败
 */
bool OstreeRepoHelper::startOstreeJob(const QString &cmd, const QString &ref, const QStringList &argList,
                                      const int timeout)
{
    QProcess process;
    process.start(cmd, argList);
    if (!process.waitForStarted()) {
        qCritical() << "start " + cmd + " failed!";
        return false;
    }

    qint64 processId = process.processId();
    // 通过script pid 查找对应的ostree pid
    if ("script" == cmd) {
        qint64 shPid = getChildPid(processId);
        qint64 ostreePid = getChildPid(shPid);
        jobMap.insert(ref, ostreePid);
    }
    if (!process.waitForFinished(timeout)) {
        qCritical() << "run " + cmd + " finish failed!";
        return false;
    }
    if ("script" == cmd) {
        jobMap.remove(ref);
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus != 0 || retCode != 0) {
        qCritical() << "run " + cmd + " failed, retCode:" << retCode << ", args:" << argList
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
        return false;
    }
    return true;
}

/*
 * 在玲珑应用安装目录创建一个临时repo子仓库
 *
 * @param parentRepo: 父repo仓库路径
 *
 * @return QString: 临时repo路径
 */
QString OstreeRepoHelper::createTmpRepo(const QString &parentRepo)
{
    QString baseDir = linglong::util::getLinglongRootPath() + "/.cache";
    linglong::util::createDir(baseDir);
    QTemporaryDir dir(baseDir + "/linglong-cache-XXXXXX");
    QString tmpPath;
    if (dir.isValid()) {
        tmpPath = dir.path();
    } else {
        qCritical() << "create tmpPath failed, please check " << baseDir << "," << dir.errorString();
        return "";
    }
    dir.setAutoRemove(false);
    auto ret = linglong::runner::Runner("ostree", {"--repo=" + tmpPath + "/repoTmp", "init", "--mode=bare-user-only"},
                                        1000 * 60 * 5);
    if (!ret) {
        return "";
    }

    // 设置最小空间要求
    ret = linglong::runner::Runner(
        "ostree", {"config", "set", "--group", "core", "min-free-space-size", "600MB", "--repo", tmpPath + "/repoTmp"},
        1000 * 60 * 5);
    if (!ret) {
        return "";
    }

    // 添加父仓库路径
    ret = linglong::runner::Runner(
        "ostree", {"config", "set", "--group", "core", "parent", parentRepo, "--repo", tmpPath + "/repoTmp"},
        1000 * 60 * 5);
    if (!ret) {
        return "";
    }

    qInfo() << "create tmp repo path:" << tmpPath << ", ret:" << QDir().exists(tmpPath + "/repoTmp");
    return tmpPath + "/repoTmp";
}

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
bool OstreeRepoHelper::repoPullbyCmd(const QString &destPath, const QString &remoteName, const QString &ref,
                                     QString &err)
{
    // 创建临时仓库
    QString tmpPath = createTmpRepo(destPath + "/repo");
    if (tmpPath.isEmpty()) {
        err = "create tmp repo err";
        qCritical() << err;
        return false;
    }

    // 将数据pull到临时仓库
    // ostree --repo=/var/tmp/linglong-cache-I80JB1/repoTmp pull --mirror repo:app/org.deepin.calculator/x86_64/1.2.2
    const QString fullref = remoteName + ":" + ref;
    // auto ret = Runner("ostree", {"--repo=" + QString(QLatin1String(tmpPath)), "pull", "--mirror", fullref}, 1000 * 60
    // * 60 * 24);
    // auto ret = startOstreeJob(ref, {"--repo=" + tmpPath, "pull", "--mirror", fullref},
    //                           1000 * 60 * 60 * 24);
    // script -q -f ostree.log -c "ostree --repo=/deepin/linglong/repo/repo pull --mirror
    // repo:com.qq.weixin.work.deepin/4.0.0.6007/x86_64"
    QString logPath = QString("/tmp/.linglong");
    linglong::util::createDir(logPath);

    // add for ll-test
    QFile file(logPath);
    QFile::Permissions permissions = file.permissions();
    permissions |= QFile::WriteOther;
    file.setPermissions(permissions);

    QString logName = ref.split("/").join("-");
    QString ostreeCmd = "ostree --repo=" + tmpPath + " pull --mirror " + fullref;
    auto ret =
        startOstreeJob("script", ref, {"-q", "-f", logPath + "/" + logName, "-c", ostreeCmd}, 1000 * 60 * 60 * 24);
    if (!ret) {
        err = "repoPullbyCmd pull error";
        qCritical() << err;
        return false;
    }
    qInfo() << "repoPullbyCmd pull success";
    // ostree --repo=test-repo(目标)  pull-local /home/linglong/work/linglong/pool/repo(源)
    // 将数据从临时仓库同步到目标仓库
    // ret = Runner("ostree", {"--repo=" + destPath + "/repo", "pull-local", QString(QLatin1String(tmpPath)), ref}, 1000
    // * 60 * 60);
    ret = startOstreeJob("ostree", ref, {"--repo=" + destPath + "/repo", "pull-local", tmpPath, ref}, 1000 * 60 * 60);

    // 删除下载进度的重定向文件
    QString filePath = "/tmp/.linglong/" + logName;
    QFile(filePath).remove();

    // 删除临时仓库
    QString tmpRepoDir = tmpPath.left(tmpPath.length() - QString("/repoTmp").length());
    qInfo() << "delete tmp repo path:" << tmpRepoDir;
    linglong::util::removeDir(tmpRepoDir);
    if (!ret) {
        err = "repoPullbyCmd pull-local error";
        qCritical() << err;
    }
    return ret;
}

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
bool OstreeRepoHelper::repoDeleteDatabyRef(const QString &repoPath, const QString &remoteName, const QString &ref,
                                           QString &err)
{
    if (repoPath.isEmpty() || remoteName.isEmpty() || ref.isEmpty()) {
        qCritical() << "repoDeleteDatabyRef param error";
        err = "repoDeleteDatabyRef param error";
        return false;
    }

    const std::string refTmp = ref.toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;

    if (!ostree_repo_set_ref_immediate(pLingLongDir->repo, NULL, refTmp.c_str(), NULL, cancellable, &error)) {
        qCritical() << "repoDeleteDatabyRef error:" << error->message;
        err = "repoDeleteDatabyRef error:" + QString(QLatin1String(error->message));
        return false;
    }
    qInfo() << "repoDeleteDatabyRef delete " << refTmp.c_str() << " success";

    gint objectsTotal;
    gint objectsPruned;
    guint64 objsizeTotal;
    g_autofree char *formattedFreedSize = NULL;
    if (!ostree_repo_prune(pLingLongDir->repo, OSTREE_REPO_PRUNE_FLAGS_REFS_ONLY, 0, &objectsTotal, &objectsPruned,
                           &objsizeTotal, cancellable, &error)) {
        qCritical() << "repoDeleteDatabyRef pruning repo failed:" << error->message;
        return false;
    }

    formattedFreedSize = g_format_size_full(objsizeTotal, (GFormatSizeFlags)0);
    qInfo() << "repoDeleteDatabyRef Total objects:" << objectsTotal;
    if (objectsPruned == 0) {
        qInfo() << "repoDeleteDatabyRef No unreachable objects";
    } else {
        qInfo() << "Deleted " << objectsPruned << " objects," << formattedFreedSize << " freed";
    }

    // const QString fullref = remoteName + ":" + ref;
    // auto ret = Runner("ostree", {"--repo=" + repoPath + "/repo", "refs", "--delete", ref}, 1000 * 60 * 30);
    // if (!ret) {
    //     qInfo() << "repoDeleteDatabyRef delete ref error";
    //     err = "repoDeleteDatabyRef delete ref error";
    //     return false;
    // }
    // ret = Runner("ostree", {"--repo=" + repoPath + "/repo", "prune", "--refs-only"}, 1000 * 60 * 30);
    // if (!ret) {
    //     qInfo() << "repoDeleteDatabyRef prune data error";
    //     err = "repoDeleteDatabyRef prune data error";
    //     return false;
    // }
    // qInfo() << "repoDeleteDatabyRef delete " << ref << " success";
    return true;
}
} // namespace linglong
