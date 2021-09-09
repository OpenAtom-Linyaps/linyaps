/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cmd.h"

#include <QJsonArray>
#include <iostream>
#include <iomanip>

void showContainer(const ContainerList &list, const QString &format)
{
    QJsonArray js;
    for (auto const &c : list) {
        js.push_back(QJsonObject {
            {"id", c->ID},
            {"pid", c->PID},
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