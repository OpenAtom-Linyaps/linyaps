/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <QString>
#include <QMap>
#include <QVector>
#include <QDebug>

using namespace std;

namespace linglong {

// ostree仓库信息
typedef struct _LingLongDir {
    GObject parent;
    string basedir;
    OstreeRepo *repo;
} LingLongDir;

const int MAX_ERRINFO_BUFSIZE = 512;
const int DEFAULT_UPDATE_FREQUENCY = 100;
#define PACKAGE_VERSION "1.0.0"
#define AT_FDCWD -100

class RepoHelper
{
public:
    RepoHelper()
    {
        mDir = new LingLongDir;
        mDir->repo = NULL;
    }
    ~RepoHelper()
    {
        if (mDir != nullptr) {
            //std::cout << "~RepoHelper() called repo:" << mDir->repo << endl;
            qInfo() << "~RepoHelper() called repo:" << mDir->repo;
            if (mDir->repo) {
                g_clear_object(&mDir->repo);
            }
            delete mDir;
            mDir = nullptr;
        }
    }

    /*
     * 创建本地repo仓库,当目标路径不存在时，自动新建路径
     *
     * @param qrepoPath: 本地仓库路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool ensureRepoEnv(const QString qrepoPath, QString &err);

    /*
     * 查询ostree远端仓库列表
     *
     * @param qrepoPath: 远端仓库对应的本地仓库路径
     * @param vec: 远端仓库列表
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    bool getRemoteRepoList(const QString qrepoPath, QVector<QString> &vec, QString &err);

    /*
     * 查询远端仓库所有软件包索引信息refs
     *
     * @param qrepoPath: 远端仓库对应的本地仓库路径
     * @param qremoteName: 远端仓库名称
     * @param outRefs: 远端仓库软件包索引信息(key:refs, value:commit值)
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    bool getRemoteRefs(const QString qrepoPath, const QString qremoteName, QMap<QString, QString> &outRefs, QString &err);

    /*
     * 查询软件包在仓库中对应的索引信息ref
     *
     * @param qrepoPath: 远端仓库对应的本地仓库路径
     * @param qremoteName: 远端仓库名称
     * @param qpkgName: 软件包包名
     * @param qarch: 架构名
     * @param matchRef: 软件包对应的索引信息ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool resolveMatchRefs(const QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &matchRef, QString &err);

    /*
     * 软件包数据从远端仓库pull到本地仓库
     *
     * @param qrepoPath: 远端仓库对应的本地仓库路径
     * @param qremoteName: 远端仓库名称
     * @param qpkgName: 软件包包名
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool repoPull(const QString qrepoPath, const QString qremoteName, const QString qpkgName, QString &err);

    /*
     * 将软件包数据从本地仓库签出到指定目录
     *
     * @param qrepoPath: 远端仓库对应的本地仓库路径
     * @param qremoteName: 远端仓库名称
     * @param qref: 软件包对应的仓库索引
     * @param qdstPath: 签出数据保存目录
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool checkOutAppData(const QString qrepoPath, const QString qremoteName, const QString qref, const QString qdstPath, QString &err);

private:
    // lint 禁止拷贝
    RepoHelper(const RepoHelper &);

    const RepoHelper &operator=(const RepoHelper &repo);

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
    void getPkgRefsBySummary(GVariant *summary, map<string, string> &outRefs);

    /*
     * 从summary中的refMap中获取仓库所有软件包索引refs
     *
     * @param ref_map: summary信息中解析出的ref map信息
     * @param outRefs: 仓库软件包索引信息
     */
    void getPkgRefsFromRefsMap(GVariant *ref_map, map<string, string> &outRefs);

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
    void splitStr(string str, string separator, vector<string> &result);

    /*
     * 解析仓库软件包索引ref信息
     *
     * @param fullRef: 目标软件包索引ref信息
     * @param result: 解析结果
     *
     * @return bool: true:成功 false:失败
     */
    bool resolveRef(const string fullRef, vector<string> &result);

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
     * @param self: 临时repo仓库对应目标仓库信息
     * @param error: 错误信息
     *
     * @return OstreeRepo: 临时repo仓库对象
     */
    OstreeRepo *createTmpRepo(LingLongDir *self, GError **error);

    /*
     * 根据指定目录创建临时repo仓库对象
     *
     * @param self: 临时repo仓库对应目标仓库信息
     * @param cachePath: 临时仓库路径
     * @param error: 错误信息
     *
     * @return OstreeRepo: 临时repo仓库对象
     */
    OstreeRepo *createChildRepo(LingLongDir *self, char *cachePath, GError **error);

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

    //ostree 仓库对象信息
    LingLongDir *mDir;
};
} // namespace linglong