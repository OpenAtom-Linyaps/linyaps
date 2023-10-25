/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_

#include "linglong/printer/printer.h"

#include <QVector>

#include <iostream>

class MockPrinter : public Printer
{

public:
    inline void printError(const int code, const QString message) override
    {
        if (code != 0) {
            this->errorList.append(code);
            std::cout << "code: " << code << " message: " << message.toStdString() << std::endl;
        }
    }

    inline void printResult(
      linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result)
      override
    {
    }

    QVector<int> errorList;
};

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_