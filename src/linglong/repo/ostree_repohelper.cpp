// /*
//  * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//  *
//  * SPDX-License-Identifier: LGPL-3.0-or-later
//  */

// #include "ostree_repohelper.h"

// #include "linglong/package/ref.h"
// #include "linglong/util/config/config.h"
// #include "linglong/util/erofs.h"
// #include "linglong/util/file.h"
// #include "linglong/util/runner.h"
// #include "linglong/util/version/version.h"
// #include "ostree-repo.h"

// #include <sys/stat.h>

// const int MAX_ERRINFO_BUFSIZE = 512;

// namespace linglong {

// /*
//  * 保存本地仓库信息
//  *
//  * @param basedir: 临时仓库路径
//  * @param repo: 错误信息
//  *
//  */
// void OstreeRepoHelper::setDirInfo(const QString &basedir, OstreeRepo *repo)
// {
//     pLingLongDir->basedir = basedir;
//     pLingLongDir->repo = repo;
// }

// OstreeRepoHelper::OstreeRepoHelper()
// {
//     pLingLongDir = new LingLongDir();
//     pLingLongDir->repo = nullptr;
// }

// OstreeRepoHelper::~OstreeRepoHelper()
// {
//     if (pLingLongDir != nullptr) {
//         qInfo() << "~OstreeRepoHelper() called";
//         // free ostree repo
//         if (pLingLongDir->repo) {
//             g_clear_object(&pLingLongDir->repo);
//         }
//         delete pLingLongDir;
//         pLingLongDir = nullptr;
//     }
// }

// /*
//  * 从 summary 中的 refMap 中获取仓库所有软件包索引 refs
//  *
//  * @param ref_map: summary 信息中解析出的 ref map 信息
//  * @param outRefs: 仓库软件包索引信息
//  */
// void OstreeRepoHelper::getPkgRefsFromRefsMap(GVariant *ref_map,
//                                              std::map<std::string, std::string> &outRefs)
// {
//     GVariant *value;
//     GVariantIter ref_iter;
//     g_variant_iter_init(&ref_iter, ref_map);
//     while ((value = g_variant_iter_next_value(&ref_iter)) != nullptr) {
//         /* helper for being able to auto-free the value */
//         g_autoptr(GVariant) child = value;
//         const char *ref_name = nullptr;
//         g_variant_get_child(child, 0, "&s", &ref_name);
//         if (ref_name == nullptr)
//             continue;
//         g_autofree char *ref = nullptr;
//         ostree_parse_refspec(ref_name, nullptr, &ref, nullptr);
//         if (ref == nullptr)
//             continue;
//         // gboolean is_app = g_str_has_prefix(ref, "app/");

//         g_autoptr(GVariant) csum_v = nullptr;
//         char tmp_checksum[65];
//         const guchar *csum_bytes;
//         g_variant_get_child(child, 1, "(t@aya{sv})", nullptr, &csum_v, nullptr);
//         csum_bytes = ostree_checksum_bytes_peek_validate(csum_v, nullptr);
//         if (csum_bytes == nullptr)
//             continue;
//         ostree_checksum_inplace_from_bytes(csum_bytes, tmp_checksum);
//         // char *newRef = g_new0(char, 1);
//         // char* newRef = NULL;
//         // newRef = g_strdup(ref);
//         // g_hash_table_insert(ret_all_refs, newRef, g_strdup(tmp_checksum));
//         outRefs.insert(std::map<std::string, std::string>::value_type(ref, tmp_checksum));
//     }
// }

// /*
//  * 从 ostree 仓库描述文件 Summary 信息中获取仓库所有软件包索引 refs
//  *
//  * @param summary: 远端仓库 Summary 信息
//  * @param outRefs: 远端仓库软件包索引信息
//  */
// void OstreeRepoHelper::getPkgRefsBySummary(GVariant *summary,
//                                            std::map<std::string, std::string> &outRefs)
// {
//     // g_autoptr(GHashTable) ret_all_refs = NULL;
//     g_autoptr(GVariant) ref_map = nullptr;
//     // g_autoptr(GVariant) metadata = NULL;

//     // ret_all_refs = g_hash_table_new(linglong_collection_ref_hash,
//     linglong_collection_ref_equal); ref_map = g_variant_get_child_value(summary, 0);
//     // metadata = g_variant_get_child_value(summary, 1);
//     getPkgRefsFromRefsMap(ref_map, outRefs);
// }

// /*
//  * 按照指定字符分割字符串
//  *
//  * @param str: 目标字符串
//  * @param separator: 分割字符串
//  * @param result: 分割结果
//  */
// void OstreeRepoHelper::splitStr(std::string str,
//                                 std::string separator,
//                                 std::vector<std::string> &result)
// {
//     size_t cutAt;
//     while ((cutAt = str.find_first_of(separator)) != str.npos) {
//         if (cutAt > 0) {
//             result.push_back(str.substr(0, cutAt));
//         }
//         str = str.substr(cutAt + 1);
//     }
//     if (str.length() > 0) {
//         result.push_back(str);
//     }
// }

// /*
//  * 解析仓库软件包索引 ref 信息
//  *
//  * @param fullRef: 目标软件包索引 ref 信息
//  * @param result: 解析结果
//  *
//  * @return bool: true:成功 false:失败
//  */
// bool OstreeRepoHelper::resolveRef(const std::string &fullRef, std::vector<std::string> &result)
// {
//     // vector<string> result;
//     splitStr(fullRef, "/", result);
//     // new ref format org.deepin.calculator/1.2.2/x86_64
//     if (result.size() != 3) {
//         qCritical() << "resolveRef Wrong number of components err";
//         return false;
//     }

//     return true;
// }

// /*
//  * 通过父进程 id 查找子进程 id
//  *
//  * @param qint64: 父进程 id
//  *
//  * @return qint64: 子进程 id
//  */
// qint64 OstreeRepoHelper::getChildPid(qint64 pid)
// {
//     QProcess process;
//     const QStringList argList = { "-P", QString::number(pid) };
//     process.start("pgrep", argList);
//     if (!process.waitForStarted()) {
//         qCritical() << "start pgrep failed!";
//         return 0;
//     }
//     if (!process.waitForFinished(1000 * 60)) {
//         qCritical() << "run pgrep finish failed!";
//         return 0;
//     }

//     auto retStatus = process.exitStatus();
//     auto retCode = process.exitCode();
//     QString childPid = "";
//     if (retStatus != 0 || retCode != 0) {
//         qCritical() << "run pgrep failed, retCode:" << retCode << ", args:" << argList
//                     << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
//                     << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
//         return 0;
//     } else {
//         childPid = QString::fromLocal8Bit(process.readAllStandardOutput());
//         qDebug() << "getChildPid ostree pid:" << childPid;
//     }

//     return childPid.toLongLong();
// }

// /*
//  * 启动一个 ostree 命令相关的任务
//  *
//  * @param cmd: 需要运行的命令
//  * @param ref: ostree 软件包对应的 ref
//  * @param argList: 参数列表
//  * @param timeout: 任务超时时间
//  *
//  * @return bool: true:成功 false:失败
//  */
// bool OstreeRepoHelper::startOstreeJob(const QString &cmd,
//                                       const QString &ref,
//                                       const QStringList &argList,
//                                       const int timeout)
// {
//     QProcess process;
//     process.start(cmd, argList);
//     if (!process.waitForStarted()) {
//         qCritical() << "start " + cmd + " failed!";
//         return false;
//     }

//     qint64 processId = process.processId();
//     // 通过 script pid 查找对应的 ostree pid
//     if ("script" == cmd) {
//         qint64 shPid = getChildPid(processId);
//         qint64 ostreePid = getChildPid(shPid);
//         jobMap.insert(ref, ostreePid); // FIXME(black_desk): ??? where is the lock???
//     }
//     if (!process.waitForFinished(timeout)) {
//         qCritical() << "run " + cmd + " finish failed!";
//         return false;
//     }
//     if ("script" == cmd) {
//         jobMap.remove(ref);
//     }

//     auto retStatus = process.exitStatus();
//     auto retCode = process.exitCode();
//     if (retStatus != 0 || retCode != 0) {
//         qCritical() << "run " + cmd + " failed, retCode:" << retCode << ", args:" << argList
//                     << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
//                     << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
//         return false;
//     }
//     return true;
// }

// /*
//  * 在玲珑应用安装目录创建一个临时 repo 子仓库
//  *
//  * @param parentRepo: 父 repo 仓库路径
//  *
//  * @return QString: 临时 repo 路径
//  */
// QString OstreeRepoHelper::createTmpRepo(const QString &parentRepo)
// {
//     QString baseDir = linglong::util::getLinglongRootPath() + "/.cache";
//     linglong::util::createDir(baseDir);
//     QTemporaryDir dir(baseDir + "/linglong-cache-XXXXXX");
//     QString tmpPath;
//     if (dir.isValid()) {
//         tmpPath = dir.path();
//     } else {
//         qCritical() << "create tmpPath failed, please check " << baseDir << ","
//                     << dir.errorString();
//         return QString();
//     }
//     dir.setAutoRemove(false);
//     auto err = util::Exec("ostree",
//                           { "--repo=" + tmpPath + "/repoTmp", "init", "--mode=bare-user-only" },
//                           1000 * 60 * 5);
//     if (err) {
//         qCritical() << "init repoTmp failed" << err;
//         return QString();
//     }

//     // 设置最小空间要求
//     err = util::Exec("ostree",
//                      { "config",
//                        "set",
//                        "--group",
//                        "core",
//                        "min-free-space-size",
//                        "600MB",
//                        "--repo",
//                        tmpPath + "/repoTmp" },
//                      1000 * 60 * 5);
//     if (err) {
//         qCritical() << "config set min-free-space-size failed" << err;
//         return QString();
//     }

//     // 添加父仓库路径
//     err = util::Exec(
//       "ostree",
//       { "config", "set", "--group", "core", "parent", parentRepo, "--repo", tmpPath + "/repoTmp"
//       }, 1000 * 60 * 5);
//     if (err) {
//         qCritical() << "config set parent failed" << err;
//         return QString();
//     }

//     qInfo() << "create tmp repo path:" << tmpPath
//             << ", ret:" << QDir().exists(tmpPath + "/repoTmp");
//     return tmpPath + "/repoTmp";
// }

// } // namespace linglong
