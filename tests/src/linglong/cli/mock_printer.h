/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_
#define LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_

#include <gmock/gmock.h>

#include "linglong/cli/printer.h"

#include <QVector>

#include <iostream>

namespace linglong::cli::test {

class MockPrinter : public Printer
{

public:
    MOCK_METHOD(void, printErr, (const utils::error::Error &), (override));
    MOCK_METHOD(void,
                printInfos,
                (const QList<QSharedPointer<linglong::package::Info>> &),
                (override));
    MOCK_METHOD(void, printContainers, (const QList<QSharedPointer<Container>> &));
};

} // namespace linglong::cli::test

#endif // LINGLONG_TEST_MODULE_CLI_FAKE_PRINTER_H_
