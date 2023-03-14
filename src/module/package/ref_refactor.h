/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_REF_H_
#define LINGLONG_SRC_MODULE_PACKAGE_REF_H_

#include <QDebug>
#include <QString>
#include <QStringList>

namespace linglong {
namespace package {

#define DEFAULT_REPO "repo"
#define DEFAULT_CHANNEL "deepin"
#define DEFAULT_MODULE "runtime"
#define DEVEL_MODULE "devel"

// local ref, {appId}/{verison}/{arch}/{module}
class Ref
{
public:
    explicit Ref(const QString &refStr)
    {
        QStringList slice = refStr.split("/");
        switch (slice.length()) {
        case 4:
            appId = slice.value(0);
            version = slice.value(1);
            arch = slice.value(2);
            module = slice.value(3);
            break;
        default:
            qCritical() << "invalid local ref" << refStr;
        }
    }

    Ref(const QString &appId,
        const QString &version,
        const QString &arch,
        const QString &module = DEFAULT_MODULE)
        : appId(appId)
        , version(version)
        , arch(arch)
        , module(module)
    {
    }

    QString toString() { return QString("%1/%2/%3/%4").arg(appId, version, arch, module); }

    Ref toDevelRef() { return Ref(appId, version, arch, DEVEL_MODULE); }

    QString appId;
    QString version;
    QString arch;
    QString module;

private:
    // TODO: now is app/runtime
    QString classify;
};

// remoteRef, {repo}:{channel}/{appId}/{verison}/{arch}/{module}
class RemoteRef : public Ref
{
public:
    explicit RemoteRef(const QString &refStr)
        : Ref(refStr.mid(refStr.indexOf("/") + 1))
    {
        // split refStr by the first position of '/' and the left substring is remoteInfo
        // it should be {repo}:{channel}
        QString remoteInfo = refStr.left(refStr.indexOf("/"));
        QStringList slice = remoteInfo.split(":");
        switch (slice.length()) {
        case 2:
            repo = slice.value(0);
            channel = slice.value(1);
            break;
        default:
            qCritical() << "invalid remote ref" << refStr;
        }
    }

    RemoteRef(const QString &repo,
              const QString &channel,
              const QString &appId,
              const QString &version,
              const QString &arch,
              const QString &module = DEFAULT_MODULE)
        : Ref(appId, version, arch, module)
        , repo(repo)
        , channel(channel)
    {
    }

    QString toString()
    {
        return QString("%1:%2/%3/%4/%5/%6").arg(repo, channel, appId, version, arch, module);
    }

    Ref toLocalRef() { return Ref(appId, version, arch, module); }

    RemoteRef toDevelRef() { return RemoteRef(repo, channel, appId, version, arch, DEVEL_MODULE); }

    static RemoteRef fromLocalRef(const Ref &ref,
                                  const QString &repo = DEFAULT_REPO,
                                  const QString &channel = DEFAULT_CHANNEL)
    {
        return RemoteRef(repo, channel, ref.appId, ref.version, ref.arch, ref.module);
    }

    QString repo;
    QString channel;
};
} // namespace package
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_PACKAGE_REF_H_ */