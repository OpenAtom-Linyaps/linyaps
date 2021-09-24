/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:
 *
 * Maintainer:
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

#include "RepoHelper.h"
#include "../util/HttpClient.h"

namespace linglong {

typedef struct MatchResult {
    AsApp *app;
    GPtrArray *remotes;
    guint score;
} MatchResult;

bool RepoHelper::ensureRepoEnv(const QString qrepoPath, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPath.empty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        //err = QString(QLatin1String(info));
        return false;
    }
    OstreeRepo *repo;
    g_autoptr(GFile) repodir = NULL;
    string tmpPath = "";
    if (repoPath.at(repoPath.size() - 1) == '/') {
        tmpPath = repoPath + "repo";
    } else {
        tmpPath = repoPath + "/repo";
    }
    fprintf(stdout, "ensureRepoEnv repo path:%s\n", tmpPath.c_str());
    repodir = g_file_new_for_path(tmpPath.c_str());
    //判断repo目录是否存在
    if (!g_file_query_exists(repodir, cancellable)) {
        fprintf(stdout, "ensureRepoEnv repo path:%s not exist create dir\n", tmpPath.c_str());
        // 创建目录
        if (g_mkdir_with_parents(tmpPath.c_str(), 0755)) {
            fprintf(stdout, "g_mkdir_with_parents fialed\n");
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s g_mkdir_with_parents fialed", __FILE__, __func__);
            //err = info;
            err = QString(QLatin1String(info));
            return false;
        }
        repo = ostree_repo_new(repodir);
        OstreeRepoMode mode = OSTREE_REPO_MODE_BARE_USER_ONLY;
        if (!ostree_repo_create(repo, mode, cancellable, &error)) {
            fprintf(stdout, "ostree_repo_create error:%s\n", error->message);
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_create error:%s", __FILE__, __func__, error->message);
            //err = info;
            err = QString(QLatin1String(info));
            return false;
        }
    } else {
        repo = ostree_repo_new(repodir);
    }

    if (!ostree_repo_open(repo, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_open:%s error:%s\n", repoPath.c_str(), error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_open:%s error:%s", __FILE__, __func__, repoPath.c_str(), error->message);
        //err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    mDir->repo = (OstreeRepo *)g_object_ref(repo);
    mDir->basedir = repoPath;
    return true;
}

bool RepoHelper::getRemoteRepoList(const QString qrepoPath, QVector<QString> &vec, QString &err)
{
    string repoPath = qrepoPath.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPath.empty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        //err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    if (!mDir->repo || mDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        //err = info;
        err = QString(QLatin1String(info));
        return false;
    }
    g_auto(GStrv) res = NULL;
    if (mDir->repo) {
        res = ostree_repo_remote_list(mDir->repo, NULL);
    }

    if (res != NULL) {
        for (int i = 0; res[i] != NULL; i++) {
            //vec.push_back(res[i]);
            vec.append(QLatin1String(res[i]));
        }
    } else {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s no romote repo found", __FILE__, __func__);
        //err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    return true;
}

bool RepoHelper::fetchRemoteSummary(OstreeRepo *repo, const char *name, GBytes **outSummary, GBytes **outSummarySig, GCancellable *cancellable, GError **error)
{
    g_autofree char *url = NULL;
    g_autoptr(GBytes) summary = NULL;
    g_autoptr(GBytes) summarySig = NULL;

    if (!ostree_repo_remote_get_url(repo, name, &url, error)) {
        fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_get_url error:%s\n", (*error)->message);
        return false;
    }
    fprintf(stdout, "fetchRemoteSummary remote %s,url:%s\n", name, url);

    if (!ostree_repo_remote_fetch_summary(repo, name, &summary, &summarySig, cancellable, error)) {
        fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_fetch_summary error:%s\n", (*error)->message);
        return false;
    }

    if (summary == NULL) {
        fprintf(stdout, "fetch summary error");
        return false;
    }
    *outSummary = (GBytes *)g_steal_pointer(&summary);
    if (outSummarySig)
        *outSummarySig = (GBytes *)g_steal_pointer(&summarySig);
    return true;
}

void RepoHelper::populate_hash_table_from_refs_map(map<string, string> &outRefs, GVariant *ref_map)
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
        //gboolean is_app = g_str_has_prefix(ref, "app/");

        g_autoptr(GVariant) csum_v = NULL;
        char tmp_checksum[65];
        const guchar *csum_bytes;
        g_variant_get_child(child, 1, "(t@aya{sv})", NULL, &csum_v, NULL);
        csum_bytes = ostree_checksum_bytes_peek_validate(csum_v, NULL);
        if (csum_bytes == NULL)
            continue;
        ostree_checksum_inplace_from_bytes(csum_bytes, tmp_checksum);
        //char *newRef = g_new0(char, 1);
        //char* newRef = NULL;
        //newRef = g_strdup(ref);
        //g_hash_table_insert(ret_all_refs, newRef, g_strdup(tmp_checksum));
        outRefs.insert(map<string, string>::value_type(ref, tmp_checksum));
    }
}

void RepoHelper::list_all_remote_refs(GVariant *summary, map<string, string> &outRefs)
{
    //g_autoptr(GHashTable) ret_all_refs = NULL;
    g_autoptr(GVariant) ref_map = NULL;
    g_autoptr(GVariant) metadata = NULL;

    //ret_all_refs = g_hash_table_new(linglong_collection_ref_hash, linglong_collection_ref_equal);
    ref_map = g_variant_get_child_value(summary, 0);
    metadata = g_variant_get_child_value(summary, 1);
    populate_hash_table_from_refs_map(outRefs, ref_map);
}

bool RepoHelper::getRemoteRefs(const QString qrepoPath, const QString qremoteName, QMap<QString, QString> &outRefs, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty()) {
        fprintf(stdout, "getRemoteRefs param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!mDir->repo || mDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    g_autoptr(GBytes) summaryBytes = NULL;
    g_autoptr(GBytes) summarySigBytes = NULL;
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    bool ret = fetchRemoteSummary(mDir->repo, remoteName.c_str(), &summaryBytes, &summarySigBytes, cancellable, &error);
    if (!ret) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    GVariant *summary = g_variant_ref_sink(g_variant_new_from_bytes(OSTREE_SUMMARY_GVARIANT_FORMAT, summaryBytes, FALSE));
    //std::map转QMap
    map<string, string> outRet;
    list_all_remote_refs(summary, outRet);
    for (auto iter = outRet.begin(); iter != outRet.end(); ++iter) {
        outRefs.insert(QString::fromStdString(iter->first), QString::fromStdString(iter->second));
    }
    return true;
}

void split(string str, string separator, vector<string> &result)
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

void no_progress_cb(OstreeAsyncProgress *progress, gpointer user_data)
{
}

//解析refs
bool RepoHelper::decompose_ref(const string fullRef, vector<string> &result)
{
    //vector<string> result;
    split(fullRef, "/", result);
    if (result.size() != 4) {
        fprintf(stdout, "Wrong number of components in %s", fullRef.c_str());
        return false;
    }

    if (result[0] != "app" && result[0] != "runtime") {
        fprintf(stdout, "%s is not application or runtime", fullRef.c_str());
        return false;
    }

    //   if (!check_valid_name (parts[1], &local_error))
    //     {
    //       fprintf (stdout, "Invalid name %s: %s", parts[1], local_error->message);
    //       return false;
    //     }

    if (result[2].size() == 0) {
        fprintf(stdout, "Invalid arch %s", result[2].c_str());
        return false;
    }

    //   if (!check_is_valid_branch (parts[3], &local_error))
    //     {
    //     }

    return true;
}

bool RepoHelper::resolveMatchRefs(const QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &matchRef, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    const string pkgName = qpkgName.toStdString();
    const string arch = qarch.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty() || pkgName.empty()) {
        fprintf(stdout, "resolveMatchRefs param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!mDir->repo || mDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    //map<string, string> outRefs;
    //bool ret = getRemoteRefs(repoPath, remoteName, outRefs, err);
    // if (ret) {
    //     for (auto it = outRefs.begin(); it != outRefs.end(); ++it) {
    //         vector<string> result;
    //         ret = decompose_ref(it->first, result);
    //         if (ret && result[1] == pkgName && result[2] == arch) {
    //             matchRef = it->first;
    //             cout << it->first << endl;
    //             cout << it->second << endl;
    //             return true;
    //         }
    //     }
    // }

    QMap<QString, QString> outRefs;
    bool ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    if (ret) {
        for (auto it = outRefs.begin(); it != outRefs.end(); ++it) {
            vector<string> result;
            ret = decompose_ref(it.key().toStdString(), result);
            //appstream 特殊处理
            if (pkgName == "appstream2" && result[0] == pkgName && result[1] == arch) {
                matchRef = it.key();
                return true;
            }
            if (ret && result[1] == pkgName && result[2] == arch) {
                matchRef = it.key();
                return true;
            }
        }
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s pkgName not found", __FILE__, __func__);
        err = info;
    }
    return false;
}

void RepoHelper::get_common_pull_options(GVariantBuilder *builder,
                                         const char *ref_to_fetch,
                                         const gchar *const *dirs_to_pull,
                                         const char *current_local_checksum,
                                         gboolean force_disable_deltas,
                                         OstreeRepoPullFlags flags,
                                         OstreeAsyncProgress *progress)
{
    guint32 update_freq = 0;
    GVariantBuilder hdr_builder;

    if (dirs_to_pull) {
        g_variant_builder_add(builder, "{s@v}", "subdirs",
                              g_variant_new_variant(g_variant_new_strv((const char *const *)dirs_to_pull, -1)));
        force_disable_deltas = TRUE;
    }

    if (force_disable_deltas) {
        g_variant_builder_add(builder, "{s@v}", "disable-static-deltas",
                              g_variant_new_variant(g_variant_new_boolean(TRUE)));
    }

    g_variant_builder_add(builder, "{s@v}", "inherit-transaction",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));

    g_variant_builder_add(builder, "{s@v}", "flags",
                          g_variant_new_variant(g_variant_new_int32(flags)));

    g_variant_builder_init(&hdr_builder, G_VARIANT_TYPE("a(ss)"));
    g_variant_builder_add(&hdr_builder, "(ss)", "ostree-Ref", ref_to_fetch);
    if (current_local_checksum)
        g_variant_builder_add(&hdr_builder, "(ss)", "linglong-Upgrade-From", current_local_checksum);
    g_variant_builder_add(builder, "{s@v}", "http-headers",
                          g_variant_new_variant(g_variant_builder_end(&hdr_builder)));
    g_variant_builder_add(builder, "{s@v}", "append-user-agent",
                          g_variant_new_variant(g_variant_new_string("linglong/" PACKAGE_VERSION)));

    if (progress != NULL)
        update_freq = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(progress), "update-frequency"));
    if (update_freq == 0)
        update_freq = DEFAULT_UPDATE_FREQUENCY;

    g_variant_builder_add(builder, "{s@v}", "update-frequency",
                          g_variant_new_variant(g_variant_new_uint32(update_freq)));
}

char *RepoHelper::repoReadLink(const char *path)
{
    char buf[PATH_MAX + 1];
    ssize_t symlink_size;

    symlink_size = readlink(path, buf, sizeof(buf) - 1);
    if (symlink_size < 0) {
        fprintf(stdout, "repoReadLink err\n");
        return NULL;
    }
    buf[symlink_size] = 0;
    return g_strdup(buf);
}

char *RepoHelper::getCacheDir()
{
    g_autofree char *path = NULL;
    g_autofree char *symlink_path = NULL;
    struct stat st_buf;
    symlink_path = g_build_filename(g_get_user_runtime_dir(), ".linglong-cache", NULL);
    path = repoReadLink(symlink_path);
    if (stat(path, &st_buf) == 0 &&
        /* Must be owned by us */
        st_buf.st_uid == getuid() &&
        /* and not writeable by others */
        (st_buf.st_mode & 0022) == 0)
        //return g_file_new_for_path(path);
        return g_strdup(path);

    path = g_strdup("/var/tmp/linglong-cache-XXXXXX");
    if (g_mkdtemp_full(path, 0755) == NULL) {
        fprintf(stdout, "Can't create temporary directory\n");
        return NULL;
    }
    unlink(symlink_path);
    if (symlink(path, symlink_path) != 0) {
        fprintf(stdout, "symlink err\n");
        return NULL;
    }
    return g_strdup(path);
}

void RepoHelper::delDirbyPath(const char *path)
{
    char cmd[512];
    pid_t result;
    memset(cmd, 0, 512);
    sprintf(cmd, "rm -rf %s", path);
    result = system(cmd);
    fprintf(stdout, "delete tmp repo, path:%s, ret:%d\n", path, result);
}

OstreeRepo *RepoHelper::createChildRepo(LingLongDir *self, char *cachePath, GError **error)
{
    g_autoptr(GFile) repo_dir = NULL;
    g_autoptr(GFile) repo_dir_config = NULL;
    OstreeRepo *repo = NULL;
    g_autofree char *tmpdir_name = NULL;
    OstreeRepo *new_repo = NULL;
    g_autoptr(GKeyFile) config = NULL;
    g_autofree char *current_mode = NULL;
    GKeyFile *orig_config = NULL;
    g_autofree char *orig_min_free_space_percent = NULL;
    g_autofree char *orig_min_free_space_size = NULL;

    OstreeRepoMode mode = OSTREE_REPO_MODE_BARE_USER_ONLY;
    const char *mode_str = "bare-user-only";

    // if (!ensureRepoEnv(self, NULL, error))
    //      return NULL;

    orig_config = ostree_repo_get_config(self->repo);

    //g_autofree char *cachePath = g_file_get_path(cache_dir);
    g_autofree char *repoTmpPath = g_strconcat(cachePath, "/repoTmp", NULL);
    fprintf(stdout, "createChildRepo repoTmpPath:%s\n", repoTmpPath);
    repo_dir = g_file_new_for_path(repoTmpPath);

    if (g_file_query_exists(repo_dir, NULL)) {
        delDirbyPath(repoTmpPath);
    }

    // 创建目录
    if (g_mkdir_with_parents(repoTmpPath, 0755)) {
        fprintf(stdout, "g_mkdir_with_parents repoTmpPath fialed\n");
        return NULL;
    }

    new_repo = ostree_repo_new(repo_dir);

    repo_dir_config = g_file_get_child(repo_dir, "config");
    if (!g_file_query_exists(repo_dir_config, NULL)) {
        if (!ostree_repo_create(new_repo, mode, NULL, error))
            return NULL;
    } else {
        /* Try to open, but on failure, re-create */
        if (!ostree_repo_open(new_repo, NULL, NULL)) {
            //rm_rf (repo_dir, NULL, NULL);
            if (!ostree_repo_create(new_repo, mode, NULL, error))
                return NULL;
        }
    }

    config = ostree_repo_copy_config(new_repo);
    /* Verify that the mode is the expected one; if it isn't, recreate the repo */
    current_mode = g_key_file_get_string(config, "core", "mode", NULL);
    if (current_mode == NULL || g_strcmp0(current_mode, mode_str) != 0) {
        //rm_rf (repo_dir, NULL, NULL);

        /* Re-initialize the object because its dir's contents have been deleted (and it
       * holds internal references to them) */
        g_object_unref(new_repo);
        new_repo = ostree_repo_new(repo_dir);

        if (!ostree_repo_create(new_repo, mode, NULL, error))
            return NULL;

        /* Reload the repo config */
        g_key_file_free(config);
        config = ostree_repo_copy_config(new_repo);
    }

    /* Ensure the config is updated */
    g_autofree char *parentRepoPath = g_file_get_path(ostree_repo_get_path(self->repo));
    g_key_file_set_string(config, "core", "parent", parentRepoPath);
    fprintf(stdout, "createChildRepo parent path:%s\n", parentRepoPath);
    /* Copy the min space percent value so it affects the temporary repo too */
    orig_min_free_space_percent = g_key_file_get_value(orig_config, "core", "min-free-space-percent", NULL);
    if (orig_min_free_space_percent)
        g_key_file_set_value(config, "core", "min-free-space-percent", orig_min_free_space_percent);
    /* Copy the min space size value so it affects the temporary repo too */
    orig_min_free_space_size = g_key_file_get_value(orig_config, "core", "min-free-space-size", NULL);
    if (orig_min_free_space_size)
        g_key_file_set_value(config, "core", "min-free-space-size", orig_min_free_space_size);

    if (!ostree_repo_write_config(new_repo, config, error))
        return NULL;

    repo = ostree_repo_new(repo_dir);
    if (!ostree_repo_open(repo, NULL, error))
        return NULL;

    /* We don't need to sync the child repos, they are never used for stable storage, and we
     verify + fsync when importing to stable storage */
    ostree_repo_set_disable_fsync(repo, TRUE);

    /* Create a commitpartial in the child repo to ensure we download everything, because
     any commitpartial state in the parent will not be inherited */
    //if (optional_commit)
    //{
    //   g_autofree char *commitpartial_basename = g_strconcat (optional_commit, ".commitpartial", NULL);
    //   g_autoptr(GFile) commitpartial =
    //   build_file (ostree_repo_get_path (repo),
    //                         "state", commitpartial_basename, NULL);
    //   g_file_replace_contents (commitpartial, "", 0, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, NULL);
    //}
    return (OstreeRepo *)g_steal_pointer(&repo);
}

OstreeRepo *RepoHelper::createTmpRepo(LingLongDir *self, GError **error)
{
    g_autofree char *cache_dir = NULL;
    cache_dir = getCacheDir();
    fprintf(stdout, "createTmpRepo cache_dir path:%s\n", cache_dir);
    if (cache_dir == NULL)
        return NULL;
    return createChildRepo(self, cache_dir, error);
}

bool RepoHelper::repoPull(const QString qrepoPath, const QString qremoteName, const QString qpkgName, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    const string pkgName = qpkgName.toStdString();

    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty() || pkgName.empty()) {
        fprintf(stdout, "repoPull param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!mDir->repo || mDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    OstreeRepoPullFlags flags = (OstreeRepoPullFlags)(OSTREE_REPO_PULL_FLAGS_MIRROR | OSTREE_REPO_PULL_FLAGS_BAREUSERONLY_FILES);
    QString qmatchRef = "";
    QString arch = "x86_64";
    bool ret = resolveMatchRefs(qrepoPath, qremoteName, qpkgName, arch, qmatchRef, err);
    if (!ret) {
        return false;
    }
    string matchRef = qmatchRef.toStdString();
    //map<string, string> outRefs;
    //string checksum = outRefs.find(matchRef)->second;
    QMap<QString, QString> outRefs;
    ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    string checksum = outRefs.find(qmatchRef).value().toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    OstreeAsyncProgress *progress = ostree_async_progress_new_and_connect(no_progress_cb, NULL);

    GVariantBuilder builder;
    g_autoptr(GVariant) options = NULL;
    const char *refs[2] = {NULL, NULL};
    const char *commits[2] = {NULL, NULL};

    refs[0] = matchRef.c_str();
    commits[0] = checksum.c_str();

    /* Pull options */
    // current_checksum "ce384fddb07dc50731858f655646da71f93fb6d6d22e9af308a5e69051b4c496"
    // refFetch "app/us.zoom.Zoom/x86_64/stable"
    // remote_name: "flathub"
    // dirs_to_pull: NULL
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    get_common_pull_options(&builder, matchRef.c_str(), NULL, checksum.c_str(),
                            0, flags, progress);
    g_variant_builder_add(&builder, "{s@v}", "refs",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-commit-ids",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)commits, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-remote-name",
                          g_variant_new_variant(g_variant_new_string(remoteName.c_str())));
    options = g_variant_ref_sink(g_variant_builder_end(&builder));
    //fprintf(stdout, "ostree_repo_pull_with_options options:%s\n", g_variant_get_data(options));
    // 下载到临时目录
    OstreeRepo *childRepo = createTmpRepo(mDir, &error);
    if (childRepo == NULL) {
        fprintf(stdout, "createTmpRepo error\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s createTmpRepo error err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!ostree_repo_prepare_transaction(childRepo, NULL, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_prepare_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_prepare_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    if (!ostree_repo_pull_with_options(childRepo, remoteName.c_str(), options,
                                       progress, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_pull_with_options error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_pull_with_options err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    if (progress) {
        ostree_async_progress_finish(progress);
    }
    if (!ostree_repo_commit_transaction(childRepo, NULL, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_commit_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_commit_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }

    //将数据从临时目录拷贝到base目录
    g_autofree char *childRepoPath = g_file_get_path(ostree_repo_get_path(childRepo));
    g_autofree char *local_url = g_strconcat("file://", childRepoPath, NULL);
    fprintf(stdout, "local_url:%s\n", local_url);
    ret = repoPullLocal(mDir->repo, remoteName.c_str(), local_url, matchRef.c_str(), checksum.c_str(), progress, cancellable, &error);
    if (!ret) {
        fprintf(stdout, "repoPullLocal error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repoPullLocal err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    delDirbyPath(childRepoPath);
    //ret = repo_pull_extra_data(mDir->repo, remoteName.c_str(), matchRef.c_str(), checksum.c_str(), err);
    return true;
}

bool RepoHelper::repoPullLocal(OstreeRepo *repo, const char *remoteName, const char *url, const char *ref, const char *checksum,
                               OstreeAsyncProgress *progress, GCancellable *cancellable, GError **error)
{
    const OstreeRepoPullFlags flags = (OstreeRepoPullFlags)(OSTREE_REPO_PULL_FLAGS_UNTRUSTED | OSTREE_REPO_PULL_FLAGS_BAREUSERONLY_FILES);
    GVariantBuilder builder;
    g_autoptr(GVariant) options = NULL;
    gboolean res;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    const char *refs[2] = {NULL, NULL};
    const char *commits[2] = {NULL, NULL};

    g_autoptr(GError) dummy_error = NULL;

    /* The ostree fetcher asserts if error is NULL */
    if (error == NULL)
        error = &dummy_error;

    g_assert(progress != NULL);

    refs[0] = ref;
    commits[0] = checksum;

    g_variant_builder_add(&builder, "{s@v}", "refs",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-commit-ids",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)commits, -1)));

    g_variant_builder_add(&builder, "{s@v}", "flags",
                          g_variant_new_variant(g_variant_new_int32(flags)));
    g_variant_builder_add(&builder, "{s@v}", "override-remote-name",
                          g_variant_new_variant(g_variant_new_string(remoteName)));
    g_variant_builder_add(&builder, "{s@v}", "gpg-verify",
                          g_variant_new_variant(g_variant_new_boolean(FALSE)));
    g_variant_builder_add(&builder, "{s@v}", "gpg-verify-summary",
                          g_variant_new_variant(g_variant_new_boolean(FALSE)));
    g_variant_builder_add(&builder, "{s@v}", "inherit-transaction",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder, "{s@v}", "update-frequency",
                          g_variant_new_variant(g_variant_new_uint32(DEFAULT_UPDATE_FREQUENCY)));

    options = g_variant_ref_sink(g_variant_builder_end(&builder));

    if (!ostree_repo_prepare_transaction(repo, NULL, cancellable, error)) {
        fprintf(stdout, "ostree_repo_prepare_transaction error:%s\n", (*error)->message);
    }

    res = ostree_repo_pull_with_options(repo, url, options,
                                        progress, cancellable, error);
    if (!res)
        //translate_ostree_repo_pull_errors(error);
        if (progress)
            ostree_async_progress_finish(progress);

    if (!ostree_repo_commit_transaction(repo, NULL, cancellable, error)) {
        fprintf(stdout, "ostree_repo_commit_transaction error:%s\n", (*error)->message);
    }
    return res;
}

GVariant *RepoHelper::get_extra_data_sources_by_commit(GVariant *commitv, GError **error)
{
    g_autoptr(GVariant) commit_metadata = NULL;
    g_autoptr(GVariant) extra_data_sources = NULL;

    commit_metadata = g_variant_get_child_value(commitv, 0);
    extra_data_sources = g_variant_lookup_value(commit_metadata, "xa.extra-data-sources",
                                                G_VARIANT_TYPE("a(ayttays)"));
    if (extra_data_sources == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "No extra data sources");
        return NULL;
    }

    return (GVariant *)g_steal_pointer(&extra_data_sources);
}

GVariant *RepoHelper::repo_get_extra_data_sources(OstreeRepo *repo, const char *rev, GCancellable *cancellable, GError **error)
{
    g_autoptr(GVariant) commitv = NULL;

    if (!ostree_repo_load_variant(repo, OSTREE_OBJECT_TYPE_COMMIT, rev, &commitv, error))
        return NULL;

    return get_extra_data_sources_by_commit(commitv, error);
}

void RepoHelper::repo_parse_extra_data_sources(GVariant *extra_data_sources, int index, const char **name,
                                               guint64 *download_size, guint64 *installed_size, const guchar **sha256, const char **uri)
{
    g_autoptr(GVariant) sha256_v = NULL;
    g_variant_get_child(extra_data_sources, index, "(^aytt@ay&s)",
                        name,
                        download_size,
                        installed_size,
                        &sha256_v,
                        uri);

    if (download_size)
        *download_size = GUINT64_FROM_BE(*download_size);

    if (installed_size)
        *installed_size = GUINT64_FROM_BE(*installed_size);

    if (sha256)
        *sha256 = ostree_checksum_bytes_peek(sha256_v);
}

bool RepoHelper::repo_pull_extra_data(OstreeRepo *repo, const char *remoteName, const char *ref, const char *commitv, string &error)
{
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    g_autoptr(GVariant) extra_data_sources = NULL;
    GCancellable *cancellable = NULL;
    extra_data_sources = repo_get_extra_data_sources(repo, commitv, cancellable, NULL);
    if (extra_data_sources == NULL) {
        fprintf(stdout, "repo_pull_extra_data extra_data_sources is null\n");
        return true;
    }
    gsize n_extra_data;
    n_extra_data = g_variant_n_children(extra_data_sources);
    if (n_extra_data == 0) {
        fprintf(stdout, "repo_pull_extra_data n_extra_data is 0\n");
        return true;
    }
    for (gsize i = 0; i < n_extra_data; i++) {
        const char *extra_data_uri = NULL;
        g_autofree char *extra_data_sha256 = NULL;
        const char *extra_data_name = NULL;
        guint64 download_size;
        guint64 installed_size;
        g_autofree char *sha256 = NULL;
        const guchar *sha256_bytes;
        g_autoptr(GBytes) bytes = NULL;
        g_autoptr(GFile) extra_local_file = NULL;
        repo_parse_extra_data_sources(extra_data_sources, i,
                                      &extra_data_name,
                                      &download_size,
                                      &installed_size,
                                      &sha256_bytes,
                                      &extra_data_uri);
        if (sha256_bytes == NULL) {
            fprintf(stdout, "Invalid checksum for extra data uri:%s\n", extra_data_uri);
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s Invalid checksum for extra data uri:%s", __FILE__, __func__, extra_data_uri);
            error = info;
            return false;
        }
        extra_data_sha256 = ostree_checksum_from_bytes(sha256_bytes);
        if (*extra_data_name == 0) {
            fprintf(stdout, "Empty name for extra data uri:%s\n", extra_data_uri);
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s Empty name for extra data uri:%s", __FILE__, __func__, extra_data_uri);
            error = info;
            return false;
        }
        // 使用http服务下载数据包
        char path[512] = {'\0'};
        getcwd(path, 512);
        linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
        //httpUtils->setProgressCallback(linglong_progress_callback);
        httpClient->loadHttpData(extra_data_uri, path);
        httpClient->release();
    }
    return true;
}

bool RepoHelper::checkOutAppData(const QString qrepoPath, const QString qremoteName, const QString qref, const QString qdstPath, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    const string ref = qref.toStdString();
    const string dstPath = qdstPath.toStdString();

    GCancellable *cancellable = NULL;
    GError *error = NULL;
    OstreeRepoCheckoutAtOptions options = {
        OSTREE_REPO_CHECKOUT_MODE_NONE,
    };
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (dstPath.empty() || ref.empty()) {
        fprintf(stdout, "checkOutAppData param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!mDir->repo || mDir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    if (g_mkdir_with_parents(dstPath.c_str(), 0755)) {
        fprintf(stdout, "g_mkdir_with_parents failed\n");
        return false;
    }

    options.mode = OSTREE_REPO_CHECKOUT_MODE_USER;
    options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    options.enable_fsync = FALSE;
    options.bareuseronly_dirs = TRUE;
    //options.subpath = "/metadata";

    //map<string, string> outRefs;
    //string checksum = outRefs.find(matchRef)->second;
    QMap<QString, QString> outRefs;
    bool ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    if (!ret) {
        return ret;
    }
    string checksum = outRefs.find(qref).value().toStdString();

    //extract_extra_data (self, checksum, extradir, &created_extra_data, cancellable, error)
    if (!ostree_repo_checkout_at(mDir->repo, &options,
                                 AT_FDCWD, dstPath.c_str(), checksum.c_str(),
                                 cancellable, &error)) {
        fprintf(stdout, "ostree_repo_checkout_at error:%s\n", error->message);
        return false;
    }
    return true;
}

bool load_appstream_store(const char *xmlPath, const char *remote_name, const char *arch, AsStore *store, GCancellable *cancellable, GError **error)
{
    g_autoptr(GFile) appstream_file = NULL;
    g_autoptr(GError) local_error = NULL;
    bool ret = false;
    //   if (arch == NULL)
    //     arch = get_arch();
    appstream_file = g_file_new_for_path(xmlPath);
    as_store_from_file(store, appstream_file, NULL, cancellable, &local_error);
    ret = (local_error == NULL);
    /* We want to ignore ENOENT error as it is harmless and valid
   * FIXME: appstream-glib doesn't have granular file-not-found error
   * See: https://github.com/hughsie/appstream-glib/pull/268 */
    if (local_error != NULL && g_str_has_suffix(local_error->message, "No such file or directory"))
        g_clear_error(&local_error);
    else if (local_error != NULL)
        g_propagate_error(error, (GError *)g_steal_pointer(&local_error));
    return ret;
}

//将appstream.xml文件加载到ASstore中
GPtrArray *getRemoteStores(const char *xmlPath, const char *remoteName, const char *arch, string &err)
{
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    GPtrArray *ret = g_ptr_array_new_with_free_func(g_object_unref);
    g_autoptr(AsStore) store = as_store_new();

#if AS_CHECK_VERSION(0, 6, 1)
    // We want to see multiple versions/branches of same app-id's, e.g. org.gnome.Platform
    as_store_set_add_flags(store, as_store_get_add_flags(store) | AS_STORE_ADD_FLAG_USE_UNIQUE_ID);
#endif
    char fullPath[512] = {'\0'};
    int len = strlen(xmlPath);
    strncpy(fullPath, xmlPath, len);
    if (strlen("appstream.xml") + len < 512) {
        if (xmlPath[len - 1] == '/') {
            strcat(fullPath, "appstream.xml");
        } else {
            strcat(fullPath, "/appstream.xml");
        }
    } else {
        fprintf(stdout, "getRemoteStores xmlPath is too long\n");
        err = "getRemoteStores xmlPath is too long";
        return NULL;
    }
    load_appstream_store(fullPath, remoteName, arch, store, cancellable, &error);
    if (error) {
        err = error->message;
        g_clear_error(&error);
    }
    g_object_set_data_full(G_OBJECT(store), "remote-name", g_strdup(remoteName), g_free);
    g_ptr_array_add(ret, g_steal_pointer(&store));
    return ret;
}

const char *as_app_get_version(AsApp *app)
{
    AsRelease *release = as_app_get_release_default(app);

    if (release)
        return as_release_get_version(release);

    return NULL;
}

const char *as_app_get_localized_name(AsApp *app)
{
    const char *const *languages = g_get_language_names();
    gsize i;
    for (i = 0; languages[i]; ++i) {
        const char *name = as_app_get_name(app, languages[i]);
        if (name != NULL)
            return name;
    }
    return NULL;
}

const char *as_app_get_localized_comment(AsApp *app)
{
    const char *const *languages = g_get_language_names();
    gsize i;
    for (i = 0; languages[i]; ++i) {
        const char *comment = as_app_get_comment(app, languages[i]);
        if (comment != NULL)
            return comment;
    }
    return NULL;
}

void print_app(MatchResult *res)
{
    const char *version = as_app_get_version(res->app);
    const char *id = as_app_get_id_filename(res->app);
    const char *name = as_app_get_localized_name(res->app);
    const char *comment = as_app_get_localized_comment(res->app);
    g_autofree char *description = g_strconcat(name, "-", comment, NULL);
    //模拟输出查询结果
    cout << version << "   " << id << "   " << name << "  " << comment << "   " << description << endl;
}

void print_matches(GSList *matches)
{
    GSList *s;
    for (s = matches; s; s = s->next) {
        MatchResult *res = (MatchResult *)s->data;
        print_app(res);
    }
}

void clear_app_arches(AsApp *app)
{
    GPtrArray *arches = as_app_get_architectures(app);

    g_ptr_array_set_size(arches, 0);
}

int compare_apps(MatchResult *a, AsApp *b)
{
    /* For now we want to ignore arch when comparing applications
   * It may be valuable to show runtime arches in the future though.
   * This is a naughty hack but for our purposes totally fine.
   */
    clear_app_arches(b);
    return !as_app_equal(a->app, b);
}

MatchResult *match_result_new(AsApp *app, guint score)
{
    MatchResult *result = g_new(MatchResult, 1);
    result->app = (AsApp *)g_object_ref(app);
    result->remotes = g_ptr_array_new_with_free_func(g_free);
    result->score = score;
    clear_app_arches(result->app);
    return result;
}

void match_result_free(MatchResult *result)
{
    g_object_unref(result->app);
    g_ptr_array_unref(result->remotes);
    g_free(result);
}

int compare_by_score(MatchResult *a, MatchResult *b, gpointer user_data)
{
    // Reverse order, higher score comes first
    return (int)b->score - (int)a->score;
}

void match_result_add_remote(MatchResult *self, const char *remote)
{
    guint i;

    for (i = 0; i < self->remotes->len; ++i) {
        const char *remote_entry = (char *)g_ptr_array_index(self->remotes, i);
        if (!strcmp(remote, remote_entry))
            return;
    }
    g_ptr_array_add(self->remotes, g_strdup(remote));
}

//根据appstream.xml文件搜索目标应用
bool RepoHelper::repoSearchApp(QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &err)
{
    bool ret = true;
    const string remoteName = qremoteName.toStdString();
    const string arch = qarch.toStdString();
    const string pkgName = qpkgName.toStdString();
    const string repoPath = qrepoPath.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty() || arch.empty() || pkgName.empty() || repoPath.empty()) {
        fprintf(stdout, "repoSearchApp param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    QString qdstPath = NULL;
    if (qrepoPath.endsWith("/")) {
        qdstPath = qrepoPath + "appstream";
    } else {
        qdstPath = qrepoPath + "/appstream";
    }
    const string dstPath = qdstPath.toStdString();
    string errInfo = "";
    GSList *matches = NULL;
    g_autoptr(GPtrArray) remote_stores = getRemoteStores(dstPath.c_str(), remoteName.c_str(), arch.c_str(), errInfo);
    for (guint j = 0; j < remote_stores->len; ++j) {
        AsStore *store = (AsStore *)g_ptr_array_index(remote_stores, j);
        GPtrArray *apps = as_store_get_apps(store);
        const char *search_text = pkgName.c_str();
        for (guint i = 0; i < apps->len; ++i) {
            AsApp *app = (AsApp *)g_ptr_array_index(apps, i);
            //fprintf(stdout, "repoSearchApp search_text:%s, appname:%s\n", search_text, as_app_get_id_filename(app));
            guint score = as_app_search_matches(app, search_text);
            if (score == 0) {
                const char *app_id = as_app_get_id_filename(app);
                if (strcasestr(app_id, search_text) != NULL)
                    score = 50;
                else
                    continue;
            }

            // Avoid duplicate entries, but show multiple remotes
            GSList *list_entry = g_slist_find_custom(matches, app,
                                                     (GCompareFunc)compare_apps);
            MatchResult *result = NULL;
            if (list_entry != NULL)
                result = (MatchResult *)list_entry->data;
            else {
                result = match_result_new(app, score);
                matches = g_slist_insert_sorted_with_data(matches, result, (GCompareDataFunc)compare_by_score, NULL);
            }
            match_result_add_remote(result, (char *)g_object_get_data(G_OBJECT(store), "remote-name"));
        }
    }

    if (matches != NULL) {
        print_matches(matches);
        g_slist_free_full(matches, (GDestroyNotify)match_result_free);
    } else {
        errInfo = "No matches found";
        ret = false;
    }
    return ret;
}

bool RepoHelper::updateAppStream(const QString qrepoPath, const QString qremoteName, const QString qarch, QString &err)
{
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (qrepoPath.isEmpty() || qremoteName.isEmpty()) {
        fprintf(stdout, "updateAppStream param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    const QString qpkgName = "appstream2";
    bool ret = repoPull(qrepoPath, qremoteName, qpkgName, err);
    if (!ret) {
        return ret;
    }
    QString qdstPath = NULL;
    if (qrepoPath.endsWith("/")) {
        qdstPath = qrepoPath + "appstream";
    } else {
        qdstPath = qrepoPath + "/appstream";
    }
    QString matchRef = "";
    ret = resolveMatchRefs(qrepoPath, qremoteName, qpkgName, qarch, matchRef, err);
    if (!ret) {
        return ret;
    }
    ret = checkOutAppData(qrepoPath, qremoteName, matchRef, qdstPath, err);
    return ret;
}
} // namespace linglong
