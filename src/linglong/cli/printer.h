/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_PRINTER_H_
#define LINGLONG_SRC_CLI_PRINTER_H_

#include "linglong/package/package.h"
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
    virtual void print(const utils::error::Error &);
    virtual void print(const QList<QSharedPointer<linglong::package::AppMetaInfo>> &);
    virtual void print(const QList<QSharedPointer<Container>> &);
};

} // namespace linglong::cli

#endif // LINGLONG_SRC_PRINTER_PRINTER_H
