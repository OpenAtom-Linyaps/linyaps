/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "job_manager.h"

#include <QDBusConnection>
#include <QTimer>
#include <QUuid>

#include "job.h"
#include "jobadaptor.h"

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

QString JobManager::CreateJob(std::function<void(Job *)> f)
{
    auto jobID = QUuid::createUuid().toString(QUuid::Id128);
    auto jobPath = "/com/deepin/linglong/Job/List/" + jobID;
    auto jr = new Job(f, this);

    // auto free with jr
    new JobAdaptor(jr);

    QDBusConnection::sessionBus().registerObject(jobPath, jr);

    return jobID;
}
