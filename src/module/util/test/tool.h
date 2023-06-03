/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <QtConcurrent/QtConcurrent>

namespace linglong::test {

void runQApplication(const std::function<void(void)> &operation)
{
    int argc = 0;
    char *argv = nullptr;
    QCoreApplication app(argc, &argv);

    QtConcurrent::run([&]() {
        auto _ = qScopeGuard([] {
            QCoreApplication::exit(0);
        });

        operation();
    });

    QCoreApplication::exec();
};

} // namespace linglong::test

