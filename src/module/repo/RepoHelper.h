#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>
#include <appstream-glib.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <QString>
#include <QMap>
#include <QVector>
using namespace std;

namespace linglong {
typedef struct ExtraData {
    char *id;
    char *displayName;
} DirExtraData;

typedef struct _LingLongDir {
    GObject parent;
    string basedir;
    //DirExtraData *extraData;
    OstreeRepo *repo;
} LingLongDir;

const int MAX_ERRINFO_BUFSIZE = 512;
const int DEFAULT_UPDATE_FREQUENCY = 100;
#define PACKAGE_VERSION "1.2.5"
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
            std::cout << "~RepoHelper() called repo:" << mDir->repo << endl;
            std::cout.flush();
            if (mDir->repo) {
                g_clear_object(&mDir->repo);
            }
            delete mDir;
            mDir = nullptr;
        }
    }

    bool ensureRepoEnv(const QString qrepoPath, QString &err);
    bool getRemoteRepoList(const QString qrepoPath, QVector<QString> &qvec, QString &err);

    bool getRemoteRefs(const QString qrepoPath, const QString qremoteName, QMap<QString, QString> &outRefs, QString &err);
    bool repoPull(const QString qrepoPath, const QString qremoteName, const QString qpkgName, QString &err);
    bool resolveMatchRefs(const QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &matchRef, QString &err);
    bool checkOutAppData(const QString qrepoPath, const QString qremoteName, const QString qref, const QString qdstPath, QString &err);

    bool updateAppStream(const QString qrepoPath, const QString qremoteName, const QString qarch, QString &err);
    bool repoSearchApp(QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &err);
private:
    bool fetchRemoteSummary(OstreeRepo *repo, const char *name, GBytes **outSummary, GBytes **outSummarySig, GCancellable *cancellable, GError **error);
    void populate_hash_table_from_refs_map(map<string, string> &outRefs, GVariant *ref_map);
    void list_all_remote_refs(GVariant *summary, map<string, string> &outRefs);
    bool decompose_ref(const string fullRef, vector<string> &result);
    void get_common_pull_options(GVariantBuilder *builder, const char *ref_to_fetch, const gchar *const *dirs_to_pull,
                                 const char *current_local_checksum, gboolean force_disable_deltas, OstreeRepoPullFlags flags, OstreeAsyncProgress *progress);
    void repo_parse_extra_data_sources(GVariant *extra_data_sources, int index, const char **name,
                                       guint64 *download_size, guint64 *installed_size, const guchar **sha256, const char **uri);

    GVariant *repo_get_extra_data_sources(OstreeRepo *repo, const char *rev, GCancellable *cancellable, GError **error);
    GVariant *get_extra_data_sources_by_commit(GVariant *commitv, GError **error);
    bool repo_pull_extra_data(OstreeRepo *repo, const char *remoteName, const char *ref, const char *commitv, string &error);
    bool repoPullLocal(OstreeRepo *repo, const char *remoteName, const char *url, const char *ref, const char *checksum, OstreeAsyncProgress *progress, GCancellable *cancellable, GError **error);
    OstreeRepo *createTmpRepo(LingLongDir *self, GError **error);
    OstreeRepo *createChildRepo(LingLongDir *self, char *cache_dir, GError **error);
    void delDirbyPath(const char *path);
    char *getCacheDir();
    char *repoReadLink(const char *path);

    LingLongDir *mDir;
};
} // namespace linglong