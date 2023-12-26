/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_client.h"

#include "linglong/package/package.h"
#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/deprecated.h"
#include "linglong/utils/error/error.h"

#include <QEventLoop>
#include <QJsonObject>

#include <tuple>

namespace linglong {
namespace repo {

using namespace api::client;

QSERIALIZER_IMPL(Response);

linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>>
RepoClient::QueryApps(const package::Ref &ref)
{
    Request_FuzzySearchReq req;
    req.setAppId(ref.appId);
    req.setVersion(ref.version);
    req.setArch(ref.arch);
    req.setRepoName(ref.repo);

    linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>> ret =
      LINGLONG_ERR(-1, "unknown error");

    QEventLoop loop;
    QEventLoop::connect(
      &client,
      &ClientApi::fuzzySearchAppSignal,
      &loop,
      [&loop, &ret](FuzzySearchApp_200_response resp) {
          loop.exit();
          if (resp.getCode() != 200) {
              ret = LINGLONG_ERR(resp.getCode(), resp.getMsg());
              return;
          }
          QJsonObject obj = resp.asJsonObject();
          QJsonDocument doc(obj);
          auto bytes = doc.toJson();
          auto respJson = util::loadJsonBytes<repo::Response>(bytes);
          ret = respJson->data;
          return;
      },
      // 当 RepoClient 不在主线程时，要使用 BlockingQueuedConnection，避免QEventLoop::exec不工作
      // TODO(wurongjie) 将RepoClient和OStreeRepo改到主线程工作
      loop.thread() == client.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
    QEventLoop::connect(
      &client,
      &ClientApi::fuzzySearchAppSignalEFull,
      &loop,
      [&loop, &ret](auto, auto error_type, const QString &error_str) {
          loop.exit();
          ret = LINGLONG_ERR(error_type, error_str);
      },
      loop.thread() == client.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
    client.fuzzySearchApp(req);
    loop.exec();
    return ret;
}

RepoClient::RepoClient(api::client::ClientApi &client, QObject *parent)
    : QObject(parent)
    , client(client)
{
}

void RepoClient::setEndpoint(const QString &endpoint)
{
    // FIXME: We should remove old server.
    this->client.setNewServerForAllOperations(endpoint);
}

} // namespace repo
} // namespace linglong
