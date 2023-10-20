/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PRINTER_PRINTER_H
#define LINGLONG_SRC_PRINTER_PRINTER_H

#include "linglong/utils/error/error.h"
#include "linglong/package/package.h"

#include <QString>
#include <QJsonObject>
#include <QObject>

#include <cstddef>

class Printer
{
public:
    virtual ~Printer() = default;
    virtual void printError(const int code , const QString message) = 0 ;
    virtual void printResult(linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result) = 0;
};

class PrinterJson : public Printer
{
public:
    void printError(const int code , const QString message) override;
    void printResult(linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result) override;
};

class PrinterNormal : public Printer
{
public:
    void printError(const int code , const QString message) override;
    void printResult(linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result) override;
};


#endif //LINGLONG_SRC_PRINTER_PRINTER_H