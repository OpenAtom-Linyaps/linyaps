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
#include <iostream>
#include <iomanip>

#include "cmd.h"

void showContainer(const ContainerList &list, const QString &format)
{
    QJsonArray js;
    for (auto const &c : list) {
        js.push_back(QJsonObject {
            {"id", c->id},
            {"pid", c->pid},
        });
    }

    if ("json" == format) {
        std::cout << QJsonDocument(js).toJson().toStdString();
    } else {
        std::cout << "\033[1m\033[38;5;214m";
        std::cout << std::left << std::setw(40) << "ContainerID"
                  << std::left << std::setw(12) << "Pid"
                  << std::left << "Path";
        std::cout << "\033[0m" << std::endl;
        for (auto const &item : js) {
            std::cout << std::left << std::setw(40) << item.toObject().value("id").toString().toStdString()
                      << std::left << std::setw(12) << item.toObject().value("pid").toInt()
                      << std::left << item.toObject().value("path").toString().toStdString()
                      << std::endl;
        }
    }
}
