/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_client.h"

#include "module/package/package.h"
#include "module/util/config/config.h"
#include "module/util/http/http_client.h"
#include "module/util/serialize/json.h"

namespace linglong {
namespace repo {

std::tuple<util::Error, package::AppMetaInfoList> RepoClient::QueryApps(const package::Ref &ref)
{
    // TODO: query cache Here
    QUrl url(ConfigInstance().repoUrl);
    url.setPath(url.path() + "apps/fuzzysearchapp");

    qDebug() << "query" << url;
    QNetworkRequest request(url);

    QJsonObject obj;
    obj["AppId"] = ref.appId;
    obj["version"] = ref.version;
    obj["arch"] = ref.arch;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    util::HttpRestClient hc;
    auto reply = hc.post(request, data);
    data = reply->readAll();
    auto resp = util::loadJsonBytes<linglong::repo::Response>(data);
    return { NoError(), (resp->data) };
}

} // namespace repo
} // namespace linglong