/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_CLI_JSON_PRINTER_H_
#define LINGLONG_CLI_JSON_PRINTER_H_

#include "linglong/cli/printer.h"
#include "linglong/dbus_ipc/reply.h"

namespace linglong::cli {

class JSONPrinter : public Printer
{
public:
    void printErr(const utils::error::Error &err) override;
    void printAppMetaInfos(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &list) override;
    void printContainers(const QList<QSharedPointer<Container>> &list) override;
    void printReply(const linglong::service::Reply &reply) override;
    void printQueryReply(const linglong::service::QueryReply &reply) override;
    void printLayerInfo(const QSharedPointer<linglong::package::Info> &info) override;
};

} // namespace linglong::cli

#endif
