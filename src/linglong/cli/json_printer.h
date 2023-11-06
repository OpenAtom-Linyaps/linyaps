/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_CLI_JSON_PRINTER_H_
#define LINGLONG_CLI_JSON_PRINTER_H_

#include "linglong/cli/printer.h"

namespace linglong::cli {

class JSONPrinter : public Printer
{
public:
    void print(const utils::error::Error &err) override;
    void print(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &list) override;
    void print(const QList<QSharedPointer<Container>> &list) override;
};

} // namespace linglong::cli

#endif
