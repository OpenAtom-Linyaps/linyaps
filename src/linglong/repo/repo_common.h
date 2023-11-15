#include <QDebug>
#include <QProcess>

#include <string>
#include <vector>

/*
 * 按照指定字符分割字符串
 *
 * @param str: 目标字符串
 * @param separator: 分割字符串
 * @param result: 分割结果
 */
static void splitStr(std::string str, std::string separator, std::vector<std::string> &result)
{

    size_t cutAt;
    while ((cutAt = str.find_first_of(separator)) != str.npos) {
        if (cutAt > 0) {
            result.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if (str.length() > 0) {
        result.push_back(str);
    }
}

/*
 * 通过父进程id查找子进程id
 *
 * @param qint64: 父进程id
 *
 * @return qint64: 子进程id
 */
static qint64 getChildPid(qint64 pid)
{
    QProcess process;
    const QStringList argList = { "-P", QString::number(pid) };
    process.start("pgrep", argList);
    if (!process.waitForStarted()) {
        qCritical() << "start pgrep failed!";
        return 0;
    }
    if (!process.waitForFinished(1000 * 60)) {
        qCritical() << "run pgrep finish failed!";
        return 0;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    QString childPid = "";
    if (retStatus != 0 || retCode != 0) {
        qCritical() << "run pgrep failed, retCode:" << retCode << ", args:" << argList
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
        return 0;
    } else {
        childPid = QString::fromLocal8Bit(process.readAllStandardOutput());
        qDebug() << "getChildPid ostree pid:" << childPid;
    }

    return childPid.toLongLong();
}