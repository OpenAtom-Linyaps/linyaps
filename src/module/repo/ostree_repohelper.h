/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <QString>
#include <QMap>
#include <QVector>
#include <QDebug>
#include <QTemporaryDir>

#include "repohelper.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"

namespace linglong {

// ostree 仓库对象信息
struct LingLongDir {
    QString basedir;
    OstreeRepo *repo;
};

class OstreeRepoHelper : public RepoHelper
    , public linglong::util::Singleton<OstreeRepoHelper>
{
public:
    OstreeRepoHelper();

    virtual ~OstreeRepoHelper();

    /*
     * 创建本地repo仓库,当目标路径不存在时，自动新建路径
     *
     * @param repoPath: 本地仓库路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool ensureRepoEnv(const QString &repoPath, QString &err);

    /*
     * 查询ostree远端仓库列表
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param vec: 远端仓库列表
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    bool getRemoteRepoList(const QString &repoPath, QVector<QString> &vec, QString &err);

    /*
     * 查询远端仓库所有软件包索引信息refs
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param remoteName: 远端仓库名称
     * @param outRefs: 远端仓库软件包索引信息(key:refs, value:commit值)
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    bool getRemoteRefs(const QString &repoPath, const QString &remoteName, QMap<QString, QString> &outRefs, QString &err);

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
    bool queryMatchRefs(const QString &repoPath, const QString &remoteName, const QString &pkgName,
                        const QString &pkgVer, const QString &arch, QString &matchRef, QString &err);

    /*
     * 软件包数据从远端仓库pull到本地仓库
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param remoteName: 远端仓库名称
     * @param pkgName: 软件包包名
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool repoPull(const QString &repoPath, const QString &remoteName, const QString &pkgName, QString &err);

    /*
     * 将软件包数据从本地仓库签出到指定目录
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引
     * @param dstPath: 签出数据保存目录
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool checkOutAppData(const QString &repoPath, const QString &remoteName, const QString &ref, const QString &dstPath, QString &err);

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
    bool repoPullbyCmd(const QString &destPath, const QString &remoteName, const QString &ref, QString &err);

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
     * 删除本地repo仓库中软件包对应的ref分支信息及数据
     *
     * @param repoPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool repoDeleteDatabyRef(const QString &repoPath, const QString &remoteName, const QString &ref, QString &err);

private:
    // multi-thread
    QMap<QString, int> jobMap;

    // lint 禁止拷贝
    OstreeRepoHelper(const OstreeRepoHelper &);

    const OstreeRepoHelper &operator=(const OstreeRepoHelper &repo);

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
    bool fetchRemoteSummary(OstreeRepo *repo, const char *name, GBytes **outSummary, GBytes **outSummarySig, GCancellable *cancellable, GError **error);

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
     * ostree 下载进度默认回调
     *
     * @param progress: OstreeAsyncProgress 对象
     * @param user_data: 用户数据
     */
    static void noProgressFunc(OstreeAsyncProgress *progress, gpointer user_data)
    {
    }

    /*
     * 按照指定字符分割字符串
     *
     * @param str: 目标字符串
     * @param separator: 分割字符串
     * @param result: 分割结果
     */
    void splitStr(std::string str, std::string separator, std::vector<std::string> &result);

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
     * 查询ostree pull参数
     *
     * @param builder: ostree pull需要的builder
     * @param ref_to_fetch: 目标软件包索引ref
     * @param dirs_to_pull: pull目标子路径
     * @param current_checksum: 软件包对应的commit值
     * @param force_disable_deltas: 禁止delta设置
     * @param flags: pull参数
     * @param progress: ostree下载进度回调
     *
     */
    void getCommonPullOptions(GVariantBuilder *builder, const char *ref_to_fetch, const gchar *const *dirs_to_pull,
                              const char *current_checksum, gboolean force_disable_deltas, OstreeRepoPullFlags flags, OstreeAsyncProgress *progress);

    /*
     * 将软件包数据从本地临时repo仓库pull到目标repo仓库
     *
     * @param repo: 远端仓库对应的本地仓库OstreeRepo对象
     * @param remoteName: 远端仓库名称
     * @param url: 临时repo仓库对应的url
     * @param ref: 目标软件包索引ref
     * @param checksum: 软件包对应的commit值
     * @param cancellable: GCancellable对象
     * @param error: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool repoPullLocal(OstreeRepo *repo, const char *remoteName, const char *url, const char *ref, const char *checksum, OstreeAsyncProgress *progress, GCancellable *cancellable, GError **error);

    /*
     * 创建临时repo仓库对象
     *
     * @param linglong_dir: 临时repo仓库对应目标仓库信息
     * @param error: 错误信息
     *
     * @return OstreeRepo: 临时repo仓库对象
     */
    OstreeRepo *createTmpRepo(LingLongDir *linglong_dir, GError **error);

    /*
     * 根据指定目录创建临时repo仓库对象
     *
     * @param linglong_dir: 临时repo仓库对应目标仓库信息
     * @param cachePath: 临时仓库路径
     * @param error: 错误信息
     *
     * @return OstreeRepo: 临时repo仓库对象
     */
    OstreeRepo *createChildRepo(LingLongDir *linglong_dir, char *cachePath, GError **error);

    /*
     * 删除临时目录
     *
     * @param path: 待删除的路径
     *
     */
    void delDirbyPath(const char *path);

    /*
     * 获取一个临时cache目录
     *
     * @return char*: 临时目录路径
     */
    char *getCacheDir();

    /*
     * 获取一个临时cache目录所指的文件路径。
     *
     * @return char*: 临时目录路径
     */
    char *repoReadLink(const char *path);

    /*
     * 保存本地仓库信息
     *
     * @param basedir: 临时仓库路径
     * @param repo: 错误信息
     *
     */
    void setDirInfo(const QString &basedir, OstreeRepo *repo);

    /*
     * 启动一个ostree 命令任务
     *
     * @param ref: ostree软件包对应的ref
     * @param argList: 参数列表
     * @param timeout: 任务超时时间
     *
     * @return bool: true:成功 false:失败
     */
    bool startOstreeJob(const QString &ref, const QStringList &argList, const int timeout);

    /*
     * 在/tmp目录下创建一个临时repo仓库
     *
     * @return QString: 临时repo路径
     */
    QString createTmpRepo();

private:
    // ostree 仓库对象信息
    LingLongDir *pLingLongDir;
};
} // namespace linglong
#define OSTREE_REPO_HELPER linglong::OstreeRepoHelper::instance()