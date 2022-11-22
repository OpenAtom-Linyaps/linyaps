/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_RUNNER_H_
#define LINGLONG_SRC_MODULE_UTIL_RUNNER_H_


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
        qCritical() << program << " init failed!";
        return false;
    }
    if (!runner.waitForFinished(timeout)) {
        qCritical() << program << " run finish failed!";
        return false;
    }
    //    auto retStatus = runner.exitStatus();
    auto retCode = runner.exitCode();
    if (retCode != 0) {
        qCritical() << program << " run failed, retCode:" << retCode << ", args:" << program << args
                    << ", info msg:" << QString::fromLocal8Bit(runner.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(runner.readAllStandardError());
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
#endif
