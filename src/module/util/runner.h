/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <QProcess>
#include <tuple>
namespace linglong {
namespace runner {

/*!
 * Runner 不带返回值的命令调用
 * @tparam T QString
 * @tparam Y QStringList
 * @param program 执行的应用程序
 * @param args 参数
 * @param timeout 30000
 * @return
 */
template<typename T = QString, typename Y = QStringList, typename P = int>
auto Runner(const T program = "ostree", const Y args = "", const P timeout = 30000) -> decltype(true)
{
    QProcess runner;
    qDebug() << program << args;
    runner.start(program, args);
    if (!runner.waitForStarted()) {
        qInfo() << program << " init failed!";
        return false;
    }
    if (!runner.waitForFinished(timeout)) {
        qInfo() << program << " run finish failed!";
        return false;
    }
    auto ret_code = runner.exitStatus();
    if (ret_code != 0) {
        qInfo() << "run failed: " << ret_code;
        return false;
    }
    return true;
};

/*!
 * RunnerRet 带返回值的命令调用
 * @tparam T
 * @tparam Y
 * @tparam P
 * @param program
 * @param args
 * @param timeout
 * @return
 */
template<typename T = QString, typename Y = QStringList, typename P = int>
std::tuple<bool, QStringList> RunnerRet(const T program = "ostree", const Y args = "", const P timeout = 30000)
{
    QProcess runner;
    qDebug() << program << args;
    runner.start(program, args);

    if (!runner.waitForStarted()) {
        qInfo() << program << " init failed!";
        return {false, QStringList()};
    }

    if (!runner.waitForFinished(timeout)) {
        qInfo() << program << " run finish failed!";
        return {false, QStringList()};
    }

    auto ret_code = runner.exitStatus();
    if (ret_code != 0) {
        qInfo() << "run failed: " << ret_code;
        return {false, QStringList()};
    }

    QString outputTxt = runner.readAllStandardOutput();

    // auto list = outputTxt.split("\n");

    return {true, outputTxt.split("\n")};
};
} // namespace runner
} // namespace linglong
