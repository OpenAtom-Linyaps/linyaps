// /*
//  * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//  *
//  * SPDX-License-Identifier: LGPL-3.0-or-later
//  */

// #ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPOHELPER_H_
// #define LINGLONG_SRC_MODULE_REPO_OSTREE_REPOHELPER_H_

// #include "linglong/repo/repo.h"
// #include "linglong/util/singleton.h"

// #include <gio/gio.h>
// #include <glib.h>
// #include <ostree-repo.h>

// #include <QDebug>
// #include <QMap>
// #include <QString>
// #include <QTemporaryDir>
// #include <QVector>

// #include <iostream>
// #include <map>
// #include <string>
// #include <vector>

// namespace linglong {
// // ostree 仓库对象信息
// struct LingLongDir
// {
//     QString basedir;
//     OstreeRepo *repo;
// };

// class OstreeRepoHelper : public repo::Repo
// {
// public:
//     OstreeRepoHelper();

//     virtual ~OstreeRepoHelper();

// private:

//     // lint 禁止拷贝
//     OstreeRepoHelper(const OstreeRepoHelper &);

//     const OstreeRepoHelper &operator=(const OstreeRepoHelper &repo);

//     /*
//      * 从ostree仓库描述文件Summary信息中获取仓库所有软件包索引refs
//      *
//      * @param summary: 远端仓库Summary信息
//      * @param outRefs: 远端仓库软件包索引信息
//      */
//     void getPkgRefsBySummary(GVariant *summary, std::map<std::string, std::string> &outRefs);

//     /*
//      * 从summary中的refMap中获取仓库所有软件包索引refs
//      *
//      * @param ref_map: summary信息中解析出的ref map信息
//      * @param outRefs: 仓库软件包索引信息
//      */
//     void getPkgRefsFromRefsMap(GVariant *ref_map, std::map<std::string, std::string> &outRefs);

//     /*
//      * 按照指定字符分割字符串
//      *
//      * @param str: 目标字符串
//      * @param separator: 分割字符串
//      * @param result: 分割结果
//      */
//     void splitStr(std::string str, std::string separator, std::vector<std::string> &result);

//     /*
//      * 解析仓库软件包索引ref信息
//      *
//      * @param fullRef: 目标软件包索引ref信息
//      * @param result: 解析结果
//      *
//      * @return bool: true:成功 false:失败
//      */
//     bool resolveRef(const std::string &fullRef, std::vector<std::string> &result);

//     /*
//      * 保存本地仓库信息
//      *
//      * @param basedir: 临时仓库路径
//      * @param repo: 错误信息
//      *
//      */
//     void setDirInfo(const QString &basedir, OstreeRepo *repo);

//     /*
//      * 启动一个ostree 命令任务
//      *
//      * @param cmd: 需要运行的命令
//      * @param ref: ostree软件包对应的ref
//      * @param argList: 参数列表
//      * @param timeout: 任务超时时间
//      *
//      * @return bool: true:成功 false:失败
//      */
//     bool startOstreeJob(const QString &cmd,
//                         const QString &ref,
//                         const QStringList &argList,
//                         const int timeout);

//     /*
//      * 在/tmp目录下创建一个临时repo子仓库
//      *
//      * @param parentRepo: 父repo仓库路径
//      *
//      * @return QString: 临时repo路径
//      */
//     QString createTmpRepo(const QString &parentRepo);

//     /*
//      * 通过父进程id查找子进程id
//      *
//      * @param qint64: 父进程id
//      *
//      * @return qint64: 子进程id
//      */
//     qint64 getChildPid(qint64 pid);

// private:
//     // ostree 仓库对象信息
//     LingLongDir *pLingLongDir;
// };
// } // namespace linglong

// #define OSTREE_REPO_HELPER linglong::OstreeRepoHelper::instance()
// #endif
