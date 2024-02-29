/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/printer.h"

#include "linglong/dbus_ipc/reply.h"

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

void Printer::printErr(const utils::error::Error &err)
{
    std::cout << "Error: " << err.message().toStdString() << std::endl;
}

void Printer::printInfos(const QList<QSharedPointer<linglong::package::Info>> &list)
{
    std::cout << "\033[38;5;214m" << std::left << std::setw(32) << qUtf8Printable("appId")
              << std::setw(32) << qUtf8Printable("name") << std::setw(16)
              << qUtf8Printable("version") << std::setw(12) << qUtf8Printable("arch")
              << std::setw(16) << qUtf8Printable("channel") << std::setw(12)
              << qUtf8Printable("module") << qUtf8Printable("description") << "\033[0m"
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
        std::cout << std::setw(32) << appId.toStdString() << std::setw(32) << name.toStdString()
                  << std::setw(16) << it->version.trimmed().toStdString() << std::setw(12)
                  << it->arch.trimmed().toStdString() << std::setw(16)
                  << it->channel.trimmed().toStdString() << std::setw(12)
                  << it->module.trimmed().toStdString() << std::setw(length)
                  << simpleDescription.toStdString() << std::endl;
    }
}

void Printer::printContainers(const QList<QSharedPointer<Container>> &list)
{
    std::cout << "\033[38;5;214m" << std::left << std::setw(48) << qUtf8Printable("App")
              << std::setw(36) << qUtf8Printable("ContainerID") << std::setw(8)
              << qUtf8Printable("Pid") << qUtf8Printable("Path") << "\033[0m" << std::endl;

    for (auto const &container : list) {
        std::cout << std::setw(48) << package::Ref(container->packageName).appId.toStdString()
                  << std::setw(36) << container->id.toStdString() << std::setw(8)
                  << QString::number(container->pid).toStdString()
                  << std::setw(container->workingDirectory.length())
                  << container->workingDirectory.toStdString() << std::endl;
    }
}

void Printer::printReply(const linglong::service::Reply &reply)
{
    std::cout << "code: " << reply.code << std::endl;
    std::cout << "message:" << std::endl << reply.message.toStdString() << std::endl;
}

void Printer::printQueryReply(const linglong::service::QueryReply &reply)
{
    std::cout << std::left << std::setw(10) << "Name";
    std::cout << "Url" << std::endl;
    std::cout << std::left << std::setw(10) << reply.message.toStdString();
    std::cout << reply.result.toStdString() << std::endl;
}

void Printer::printLayerInfo(const QSharedPointer<linglong::package::Info> &info)
{
    // some info are not printed, such as base
    std::cout << "AppID: " << info->appId.toStdString() << std::endl;
    std::cout << "Name: " << info->name.toStdString() << std::endl;
    std::cout << "Kind: " << info->kind.toStdString() << std::endl;
    std::cout << "Version: " << info->version.toStdString() << std::endl;
    std::cout << "Arch: " << info->arch.toStdString() << std::endl;
    std::cout << "Module: " << info->module.toStdString() << std::endl;
    std::cout << "Runtime: " << info->runtime.toStdString() << std::endl;
    std::cout << "Size: " << info->size << " bytes" << std::endl;
    std::cout << "Description: " << info->description.toStdString() << std::endl;
}

void Printer::printMessage(const QString &text, const int num)
{
    QByteArray blank;
    blank.fill(' ', num);

    std::cout << blank.toStdString() << text.toStdString() << std::endl;
}

void Printer::printReplacedText(const QString &text, const int num)
{
    QByteArray blank;
    blank.fill(' ', num);

    std::cout << "\33[2K\r" << blank.toStdString() << text.toStdString() << std::flush;
}

} // namespace linglong::cli
