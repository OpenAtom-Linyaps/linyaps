/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "module/repo/ostree_repohelper.h"
#include "job_manager.h"

#include <QDBusConnection>
#include <QTimer>
#include <QUuid>

#include "job.h"

class JobManagerPrivate
{
public:
    explicit JobManagerPrivate(JobManager *parent)
        : q_ptr(parent)
    {
    }

    JobManager *q_ptr = nullptr;
};

JobManager::JobManager() = default;

JobManager::~JobManager() = default;

QString JobManager::CreateJob(std::function<void()> f)
{
    auto jobId = QUuid::createUuid().toString(QUuid::Id128);
    auto jobPath = "/com/deepin/linglong/Job/List/" + jobId;
    auto jr = new Job(f, this);
    jr->start();

    return jobId;
}

// dbus-send --session --type=method_call --print-reply --dest=com.deepin.linglong.AppManager
// /com/deepin/linglong/JobManager com.deepin.linglong.JobManager.Start string:"org.deepin.demo"
void JobManager::Start(const QString &jobId)
{
    if (jobId.isNull() || jobId.isEmpty()) {
        qCritical() << "jobID err";
        return;
    }

    int processId = OSTREE_REPO_HELPER->getOstreeJobId(jobId);
    if (processId == -1) {
        qWarning() << jobId << " not exist";
        return;
    }
    qInfo() << "restart job:" << jobId << ", process:" << processId;
    QProcess process;
    QStringList argStrList = {"-CONT", QString::number(processId)};
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

    int processId = OSTREE_REPO_HELPER->getOstreeJobId(jobId);
    if (processId == -1) {
        qWarning() << jobId << " not exist";
        return;
    }
    qInfo() << "pause job:" << jobId << ", process:" << processId;
    QProcess process;
    QStringList argStrList = {"-STOP", QString::number(processId)};
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
