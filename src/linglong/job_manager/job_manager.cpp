/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/job_manager/job_manager.h"

#include "linglong/job_manager/job.h"
#include "linglong/repo/ostree_repo.h"

#include <QDBusConnection>
#include <QProcess>
#include <QTimer>
#include <QUuid>

namespace linglong::job_manager {

JobManager::JobManager(linglong::repo::OSTreeRepo &ostreeRepo, QObject *parent)
    : QObject(parent)
    , ostreeRepo(ostreeRepo)
{
}

QString JobManager::CreateJob(std::function<void()> f)
{
    auto jobId = QUuid::createUuid().toString(QUuid::Id128);
    auto jobPath = "/org/deepin/linglong/Job/List/" + jobId;
    auto jr = new Job(f);
    jr->start();

    return jobId;
}

// dbus-send --system --type=method_call --print-reply --dest=org.deepin.linglong.PackageManager
// /org/deepin/linglong/JobManager org.deepin.linglong.JobManager.Start string:"org.deepin.demo"
void JobManager::Start(const QString &jobId)
{
    if (jobId.isNull() || jobId.isEmpty()) {
        qCritical() << "jobID err";
        return;
    }

    int processId = ostreeRepo.getOstreeJobId(jobId);
    if (processId == -1) {
        qWarning() << jobId << " not exist";
        return;
    }
    qInfo() << "restart job:" << jobId << ", process:" << processId;
    QProcess process;
    QStringList argStrList = { "-CONT", QString::number(processId) };
    process.start("kill", argStrList);
    if (!process.waitForStarted()) {
        qCritical() << "kill -CONT failed!";
        return;
    }
    if (!process.waitForFinished(3000)) {
        qCritical() << "run finish failed!";
        return;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus != 0 || retCode != 0) {
        qCritical() << " run failed, retCode:" << retCode
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
    }
}

// 下载应用的时候 正在下载runtime 如何停止？
void JobManager::Stop(const QString &jobId)
{
    if (jobId.isNull() || jobId.isEmpty()) {
        qCritical() << "jobId err";
        return;
    }

    int processId = ostreeRepo.getOstreeJobId(jobId);
    if (processId == -1) {
        qWarning() << jobId << " not exist";
        return;
    }
    qInfo() << "pause job:" << jobId << ", process:" << processId;
    QProcess process;
    QStringList argStrList = { "-STOP", QString::number(processId) };
    process.start("kill", argStrList);
    if (!process.waitForStarted()) {
        qCritical() << "kill -STOP failed!";
        return;
    }
    if (!process.waitForFinished(3000)) {
        qCritical() << "run finish failed!";
        return;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus != 0 || retCode != 0) {
        qCritical() << " run failed, retCode:" << retCode
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
    }
}

// Fix to do 取消之后再下载问题
void JobManager::Cancel(const QString &jobId)
{
    if (jobId.isNull() || jobId.isEmpty()) {
        qCritical() << "jobId err";
        return;
    }

    int processId = ostreeRepo.getOstreeJobId(jobId);
    if (processId == -1) {
        qWarning() << jobId << " not exist";
        return;
    }

    qInfo() << "cancel job:" << jobId << ", process:" << processId;
    QProcess process;
    QStringList argStrList = { "-9", QString::number(processId) };
    process.start("kill", argStrList);
    if (!process.waitForStarted()) {
        qCritical() << "kill failed!";
        return;
    }
    if (!process.waitForFinished(3000)) {
        qCritical() << "run finish failed!";
        return;
    }

    auto retStatus = process.exitStatus();
    auto retCode = process.exitCode();
    if (retStatus != 0 || retCode != 0) {
        qCritical() << " run failed, retCode:" << retCode
                    << ", info msg:" << QString::fromLocal8Bit(process.readAllStandardOutput())
                    << ", err msg:" << QString::fromLocal8Bit(process.readAllStandardError());
    }
}

QStringList JobManager::List()
{
    return ostreeRepo.getOstreeJobList();
}

} // namespace linglong::job_manager
