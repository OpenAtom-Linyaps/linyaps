/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/package/remote_ref.h"

#include "ref_new.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace linglong {
namespace package {
namespace refact {

namespace defaults {
const QString repo = "repo";
const QString channel = "stablee";
} // namespace defaults

RemoteRef::RemoteRef(const QString &str)
    : Ref([&]() -> QString {
        auto components = str.split(":");
        if (components.length() > 1)
            return str.split(":")[1];
        else
            return "";
    }())
{
    if (!this->Ref::isVaild()) {
        return;
    }

    auto components = str.split(":");
    if (components.length() < 1)
        return;

    auto slice = components[0].split("/");

    if (slice.length() >= 3)
        return;

    switch (slice.length()) {
    case 2:
        channel = slice.value(1);
        [[fallthrough]];
    case 1:
        repo = slice.value(0);
    }

    if (repo.isEmpty()) {
        repo = defaults::repo;
    }

    if (channel.isEmpty()) {
        channel = defaults::channel;
    }
}

RemoteRef::RemoteRef(const QString &appId,
                     const QString &version,
                     const QString &arch,
                     const QString &module,
                     const QString &repo,
                     const QString &channel)
    : Ref(appId, version, arch, module)
    , repo(repo)
    , channel(channel)
{
}

RemoteRef::RemoteRef(const Ref &ref, const QString &repo, const QString &channel)
    : Ref(ref)
    , repo(repo)
    , channel(channel)
{
}

QString RemoteRef::toString() const
{
    return QString("%1/%2:%3/%4/%5/%6").arg(repo, channel, packageID, version, arch, module);
}

bool RemoteRef::isVaild() const
{
    return this->Ref::isVaild() && [&]() -> bool {
        QRegularExpression regexExp(verifyRegexp);

        auto repoMatched = regexExp.match(this->repo);
        auto channelMatched = regexExp.match(this->repo);
        return repoMatched.hasMatch() && channelMatched.hasMatch();
    }();
}

} // namespace refact
} // namespace package
} // namespace linglong
