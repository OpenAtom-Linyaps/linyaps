/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include "linglong/dbus_ipc/reply.h"

#include <QJsonArray>

#include <iostream>

namespace linglong::cli {

void JSONPrinter::print(const utils::error::Error &err)
{
    QJsonObject obj;
    obj["code"] = err.code();
    obj["message"] = err.message();
    std::cout << QString::fromUtf8(QJsonDocument(obj).toJson()).toStdString();
}

void JSONPrinter::print(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &list)
{
    QJsonArray array;
    for (const auto &it : list) {
        QJsonObject obj;
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
        array.push_back(obj);
    }
    std::cout << QString::fromUtf8(QJsonDocument(array).toJson()).toStdString() << std::endl;
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

    std::cout << QString::fromUtf8(QJsonDocument(jsonArray).toJson()).toStdString() << std::endl;
}

void JSONPrinter::print(const linglong::service::Reply &reply)
{
    QJsonArray jsonArray;
    jsonArray.push_back(QJsonObject{
      { "code", reply.code },
      { "message", reply.message },
    });

    std::cout << QString::fromUtf8(QJsonDocument(jsonArray).toJson()).toStdString() << std::endl;
}

void JSONPrinter::print(const linglong::service::QueryReply &reply)
{
    QJsonArray jsonArray;
    jsonArray.push_back(QJsonObject{
      { "name", reply.message },
      { "url", reply.result },
    });

    std::cout << QString::fromUtf8(QJsonDocument(jsonArray).toJson()).toStdString() << std::endl;
}

} // namespace linglong::cli
