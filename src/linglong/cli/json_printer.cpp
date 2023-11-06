/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include <QJsonArray>

namespace linglong::cli {

void JSONPrinter::print(const utils::error::Error &err)
{
    QJsonObject obj;
    obj["code"] = err.code();
    obj["message"] = err.message();
    qCritical().noquote() << QJsonDocument(obj);
}

void JSONPrinter::print(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &list)
{
    QJsonObject obj;
    for (const auto &it : list) {
        obj["appId"] = it->appId.trimmed();
        obj["name"] = it->name.trimmed();
        obj["version"] = it->version.trimmed();
        obj["arch"] = it->arch.trimmed();
        obj["kind"] = it->kind.trimmed();
        obj["runtime"] = it->runtime.trimmed();
        obj["uabUrl"] = it->uabUrl.trimmed();
        obj["repoName"] = it->repoName.trimmed();
        obj["description"] = it->description.trimmed();
        obj["user"] = it->user.trimmed();
        obj["size"] = it->size.trimmed();
        obj["channel"] = it->channel.trimmed();
        obj["module"] = it->module.trimmed();
    }
    qInfo().noquote() << QJsonDocument(obj);
}

void JSONPrinter::print(const QList<QSharedPointer<Container>> &list)
{
    QJsonArray jsonArray;
    for (auto const &container : list) {
        jsonArray.push_back(QJsonObject{
          { "app", package::Ref(container->packageName).appId },
          { "id", container->id },
          { "pid", container->pid },
          { "path", container->workingDirectory },
        });
    }

    qInfo().noquote() << QJsonDocument(jsonArray).toJson();
}
} // namespace linglong::cli
