/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QJsonArray>

#include "cmd.h"

void showContainer(const ContainerList &list, const QString &format)
{
    QJsonArray js;
    for (auto const &c : list) {
        js.push_back(QJsonObject {
            {"app", c->packageName},
            {"id", c->id},
            {"pid", c->pid},
            {"path", c->workingDirectory},
        });
    }

    if ("json" == format) {
        qInfo().noquote() << QJsonDocument(js).toJson();
    } else {
        qInfo("\033[1m\033[38;5;214m%-48s%-36s%-8s%-s\033[0m", "App", "ContainerID", "Pid", "Path");
        for (auto const &item : js) {
            QString path = item.toObject().value("path").toString();
            qInfo().noquote() << QString("%1%2%3%4")
                                     .arg(item.toObject().value("app").toString(), -48, QLatin1Char(' '))
                                     .arg(item.toObject().value("id").toString(), -36, QLatin1Char(' '))
                                     .arg(QString::number(item.toObject().value("pid").toInt()), -8, QLatin1Char(' '))
                                     .arg(path, -path.length(), QLatin1Char(' '));
        }
    }
}
