/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "repo_client.h"

#include "module/util/config/config.h"
#include "module/util/http/http_client.h"
#include "module/util/json.h"
#include "module/package/package.h"

namespace linglong {
namespace repo {

std::tuple<util::Error, package::AppMetaInfoList> RepoClient::QueryApps(const package::Ref &ref)
{
    // TODO: query cache Here
    QUrl url(ConfigInstance().repoUrl);
    url.setPath(url.path() + "apps/fuzzysearchapp");

    QNetworkRequest request(url);

    QJsonObject obj;
    obj["AppId"] = ref.appId;
    obj["version"] = ref.version;
    obj["arch"] = ref.arch;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    util::HttpRestClient hc;
    qCritical() << "start";
    auto reply = hc.post(request, data);
    qCritical() << "start do";
    data = reply->readAll();
    qCritical() << "readall";
    qCritical().noquote() << QString::fromLocal8Bit(data);
    auto resp = util::loadJsonBytes<linglong::repo::Response>(data);

    return {NoError(), (resp->data)};
}

} // namespace repo
} // namespace linglong