/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_PRINTER_H_
#define LINGLONG_SRC_CLI_PRINTER_H_

#include "linglong/dbus_ipc/reply.h"
#include "linglong/package/info.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/error/error.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <cstddef>

namespace linglong::cli {

class Printer
{
public:
    Printer() = default;
    Printer(const Printer &) = delete;
    Printer(Printer &&) = delete;
    Printer &operator=(const Printer &) = delete;
    Printer &operator=(Printer &&) = delete;
    virtual ~Printer() = default;
    virtual void printErr(const utils::error::Error &);
    virtual void printInfos(const QList<QSharedPointer<linglong::package::Info>> &);
    virtual void printContainers(const QList<QSharedPointer<Container>> &);
    virtual void printReply(const linglong::service::Reply &);
    virtual void printQueryReply(const linglong::service::QueryReply &);
    virtual void printLayerInfo(const QSharedPointer<linglong::package::Info> &);

    virtual void printMessage(const QString &, const int num = -1);
    virtual void printReplacedText(const QString &, const int num = -1);
};

} // namespace linglong::cli

#endif // LINGLONG_SRC_PRINTER_PRINTER_H
