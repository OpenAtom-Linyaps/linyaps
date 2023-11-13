/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_

#include "linglong/package/package.h"
#include "linglong/repo/repo.h"

#include <QPointer>
#include <QScopedPointer>

namespace linglong {
namespace repo {

class OSTreeRepoPrivate;

class OSTreeRepo : public QObject, public Repo
{
    Q_OBJECT
public:
    enum Mode {
        Bare,
        BareUser,
        BareUserOnly,
        Archive,
    };

    Q_ENUM(Mode);

    explicit OSTreeRepo(const QString &path, QObject *parent = nullptr);
    explicit OSTreeRepo(const QString &localRepoPath,
                        const QString &remoteEndpoint,
                        const QString &remoteRepoName,
                        QObject *parent = nullptr);

    ~OSTreeRepo() override;
    linglong::util::Error init(const QString &repoMode);

    linglong::util::Error remoteAdd(const QString &repoName, const QString &repoUrl);

    std::tuple<linglong::util::Error, QStringList> remoteList();

    linglong::util::Error importDirectory(const package::Ref &ref, const QString &path) override;

    linglong::util::Error renameBranch(const package::Ref &oldRef, const package::Ref &newRef);

    linglong::util::Error import(const package::Bundle &bundle) override;

    linglong::util::Error exportBundle(package::Bundle &bundle) override;

    std::tuple<linglong::util::Error, QList<package::Ref>> list(const QString &filter) override;

    std::tuple<linglong::util::Error, QList<package::Ref>> query(const QString &filter) override;

    linglong::util::Error push(const package::Ref &ref, bool force) override;

    linglong::util::Error push(const package::Ref &ref) override;

    linglong::util::Error push(const package::Bundle &bundle, bool force) override;

    linglong::util::Error pull(const package::Ref &ref, bool force) override;

    linglong::util::Error pullAll(const package::Ref &ref, bool force);

    linglong::util::Error checkout(const package::Ref &ref,
                                   const QString &subPath,
                                   const QString &target);

    linglong::util::Error removeRef(const package::Ref &ref);

    linglong::util::Error remoteDelete(const QString &repoName);

    linglong::util::Error checkoutAll(const package::Ref &ref,
                                      const QString &subPath,
                                      const QString &target);

    std::tuple<QString, util::Error> compressOstreeData(const package::Ref &ref);

    QString rootOfLayer(const package::Ref &ref) override;

    bool isRefExists(const package::Ref &ref);

    QString remoteShowUrl(const QString &repoName);

    package::Ref localLatestRef(const package::Ref &ref);

    package::Ref remoteLatestRef(const package::Ref &ref);

    package::Ref latestOfRef(const QString &appId, const QString &appVersion) override;

private:
    QScopedPointer<OSTreeRepoPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), OSTreeRepo)
};

} // namespace repo
} // namespace linglong

namespace linglong {
namespace repo {

class InfoResponse : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(InfoResponse)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QString, msg);
    Q_JSON_PROPERTY(ParamStringMap, revs);
};

class RevPair : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(RevPair)

    Q_JSON_PROPERTY(QString, server);
    Q_JSON_PROPERTY(QString, client);
};

class UploadResponseData : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadResponseData)

    Q_JSON_PROPERTY(QString, id);
    Q_JSON_PROPERTY(QString, watchId);
    Q_JSON_PROPERTY(QString, status);
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, InfoResponse)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, RevPair)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadResponseData)

namespace linglong {
namespace repo {

class UploadTaskRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadTaskRequest)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QStringList, objects);
    Q_PROPERTY(QMap<QString, QSharedPointer<linglong::repo::RevPair>> refs MEMBER refs);
    QMap<QString, QSharedPointer<linglong::repo::RevPair>> refs;
};

class UploadRequest : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadRequest)

    Q_JSON_PROPERTY(QString, repoName);
    Q_JSON_PROPERTY(QString, ref);
};

class UploadTaskResponse : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(UploadTaskResponse)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QString, msg);
    Q_JSON_PTR_PROPERTY(linglong::repo::UploadResponseData, data);
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadRequest)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadTaskRequest)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, UploadTaskResponse)

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
