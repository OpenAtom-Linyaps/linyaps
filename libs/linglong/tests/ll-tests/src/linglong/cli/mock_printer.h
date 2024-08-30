/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

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
                printAppMetaInfos,
                (const QList<QSharedPointer<linglong::package::AppMetaInfo>> &),
                (override));
    MOCK_METHOD(void, printContainers, (const QList<QSharedPointer<Container>> &));
};

} // namespace linglong::cli::test
