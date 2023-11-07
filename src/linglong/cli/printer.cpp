/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/printer.h"

#include <QJsonArray>

#include <iomanip>
#include <iostream>

namespace linglong::cli {
namespace {
/**
 * @brief 统计字符串中中文字符的个数
 *
 * @param name 软件包名称
 * @return int 中文字符个数
 */
int getUnicodeNum(const QString &name)
{
    int num = 0;
    int count = name.count();
    for (int i = 0; i < count; i++) {
        QChar ch = name.at(i);
        ushort decode = ch.unicode();
        if (decode >= 0x4E00 && decode <= 0x9FA5) {
            num++;
        }
    }
    return num;
}
} // namespace

void Printer::print(const utils::error::Error &err)
{
    qCritical().noquote() << "code:" << err.code() << "message:" << err.message();
}

void Printer::print(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &list)
{
    if (list.empty()) {
        qInfo().noquote() << "app not found in repo";
        return;
    }

    // qInfo("\033[1m\033[38;5;214m%-32s%-32s%-16s%-12s%-16s%-12s%-s\033[0m",
    //       qUtf8Printable("appId"),
    //       qUtf8Printable("name"),
    //       qUtf8Printable("version"),
    //       qUtf8Printable("arch"),
    //       qUtf8Printable("channel"),
    //       qUtf8Printable("module"),
    //       qUtf8Printable("description"));
    const std::string color("\033[38;5;214m");

    std::cout << std::setw(32) << qUtf8Printable("appId") << std::setw(32) << qUtf8Printable("name")
              << std::setw(16) << qUtf8Printable("version") << std::setw(12)
              << qUtf8Printable("arch") << std::setw(16) << qUtf8Printable("channel")
              << std::setw(12) << qUtf8Printable("module") << qUtf8Printable("description") << color
              << std::endl;

    for (const auto &it : list.toVector()) {
        QString simpleDescription = it->description.trimmed();
        if (simpleDescription.length() > 56) {
            simpleDescription = it->description.trimmed().left(53) + "...";
        }
        QString appId = it->appId.trimmed();
        QString name = it->name.trimmed();
        if (name.length() > 32) {
            name = it->name.trimmed().left(29) + "...";
        }
        if (appId.length() > 32) {
            name.push_front(" ");
        }
        int count = getUnicodeNum(name);
        int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
        // qInfo().noquote() << QString("%1%2%3%4%5%6%7")
        //                        .arg(appId, -32, QLatin1Char(' '))
        //                        .arg(name, count - 32, QLatin1Char(' '))
        //                        .arg(it->version.trimmed(), -16, QLatin1Char(' '))
        //                        .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
        //                        .arg(it->channel.trimmed(), -16, QLatin1Char(' '))
        //                        .arg(it->module.trimmed(), -12, QLatin1Char(' '))
        //                        .arg(simpleDescription, -length, QLatin1Char(' '));
        std::cout << std::setw(32) << appId.toLocal8Bit().data() << std::setw(32)
                  << name.toLocal8Bit().data() << std::setw(16)
                  << it->version.trimmed().toLocal8Bit().data() << std::setw(12)
                  << it->arch.trimmed().toLocal8Bit().data() << std::setw(16)
                  << it->channel.trimmed().toLocal8Bit().data() << std::setw(12)
                  << it->module.trimmed().toLocal8Bit().data() << std::setw(length)
                  << simpleDescription.toLocal8Bit().data() << std::endl;
    }
}

void Printer::print(const QList<QSharedPointer<Container>> &list)
{
    const std::string color("\033[38;5;214m");

    std::cout << std::setw(48) << qUtf8Printable("App") << std::setw(36)
              << qUtf8Printable("ContainerID") << std::setw(8) << qUtf8Printable("Pid")
              << qUtf8Printable("Path") << color << std::endl;

    // qInfo("\033[1m\033[38;5;214m%-48s%-36s%-8s%-s\033[0m", "App", "ContainerID", "Pid", "Path");
    for (auto const &container : list) {
        // qInfo().noquote() << QString("%1%2%3%4")
        //                        .arg(package::Ref(container->packageName).appId,
        //                             -48,
        //                             QLatin1Char(' '))
        //                        .arg(container->id, -36, QLatin1Char(' '))
        //                        .arg(container->pid, -8, QLatin1Char(' '))
        //                        .arg(container->workingDirectory,
        //                             -container->workingDirectory.length(),
        //                             QLatin1Char(' '));
        std::cout << std::setw(48)
                  << package::Ref(container->packageName).appId.toLocal8Bit().data()
                  << std::setw(36) << container->id.toLocal8Bit().data() << std::setw(8)
                  << QString::number(container->pid).toLocal8Bit().data()
                  << std::setw(container->workingDirectory.length())
                  << container->workingDirectory.toLocal8Bit().data() << std::endl;
    }
}

} // namespace linglong::cli
