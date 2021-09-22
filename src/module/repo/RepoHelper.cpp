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

// 暂时使用全局变量
//LingLongDir dir;
//LingLongDir *RepoHelper::mDir = &dir;

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
        char *ref = NULL;
        ostree_parse_refspec(ref_name, NULL, &ref, NULL);
        if (ref == NULL)
            continue;
        gboolean is_app = g_str_has_prefix(ref, "app/");
        //fprintf(stdout, "populate_hash_table_from_refs_map ref_name:%s, ref:%s\n", ref_name, ref);

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
    int cutAt;
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
            if (ret && result[1] == pkgName && result[2] == arch) {
                matchRef = it.key();
                return true;
            }
        }
    }
    snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s pkgName not found", __FILE__, __func__);
    err = info;
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
    fprintf(stdout, "ostree_repo_pull_with_options options:%s\n", g_variant_get_data(options));

    if (!ostree_repo_prepare_transaction(mDir->repo, NULL, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_prepare_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_prepare_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }

    if (!ostree_repo_pull_with_options(mDir->repo, remoteName.c_str(), options,
                                       progress, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_pull_with_options error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_pull_with_options err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }

    if (progress) {
        ostree_async_progress_finish(progress);
    }
    if (!ostree_repo_commit_transaction(mDir->repo, NULL, cancellable, &error)) {
        fprintf(stdout, "ostree_repo_commit_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_commit_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }

    //ret = repo_pull_extra_data(mDir->repo, remoteName.c_str(), matchRef.c_str(), checksum.c_str(), err);
    return true;
}

bool repoPullLocal(OstreeRepo *repo, const char *remote_name, const char *url, const char *ref, const char *checksum,
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
                          g_variant_new_variant(g_variant_new_string(remote_name)));
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

// OstreeRepo* RepoHelper::create_system_child_repo(LingLongDir *self, const char *optional_commit, GError **error) {
//     g_autoptr(GFile) cache_dir = NULL;
//     cache_dir = flatpak_ensure_system_user_cache_dir_location(error);
//     fprintf(stdout, "create_system_child_repo cache_dir path:%s\n", g_file_get_path(cache_dir));

//     if (cache_dir == NULL)
//         return NULL;

//     return flatpak_dir_create_child_repo(self, cache_dir, NULL, error);
// }

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
    for (int i = 0; i < n_extra_data; i++) {
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
        const char *savePath = "/home/xxxx/test1";
        linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
        //httpUtils->setProgressCallback(linglong_progress_callback);
        httpClient->loadHttpData(extra_data_uri, savePath);
        httpClient->release();
    }
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
} // namespace linglong
