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
